// Baud-rate finder: listens at each candidate setting and scores what arrives. Passive unless
// the owner turns on the probe for this run.
#include "tool_baud.h"
#include "core/baudscore.h"

#define WINDOW_MS 1500

typedef enum { StateIdle, StateRunning, StateDone } State;

typedef struct {
    State state;
    bool probe;
    size_t index;
    uint32_t window_start;
    BaudSample sample;
    uint8_t scores[16];
    uint32_t bytes[16];
    uint8_t best[3];
    SerialConfig saved;
    char status[32];
} BaudModel;

static void label(const SerialConfig* c, char* out, size_t size) {
    serial_config_label(c, out, size);
}

static void draw(Canvas* canvas, void* model) {
    BaudModel* m = model;
    char buf[48], cfg[24];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Baud-rate finder");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 12, 127, 12);
    if(m->state == StateIdle) {
        canvas_draw_str(canvas, 2, 23, "The device must be sending.");
        canvas_draw_str(canvas, 2, 32, "Listens 1.5 s at 16 settings.");
        snprintf(buf, sizeof(buf), "Send a probe (CR): %s  <>", m->probe ? "on" : "off");
        canvas_draw_str(canvas, 2, 44, buf);
        canvas_draw_str(canvas, 2, 63, m->status[0] ? m->status : "OK: start");
    } else if(m->state == StateRunning) {
        label(&baud_candidates[m->index], cfg, sizeof(cfg));
        snprintf(buf, sizeof(buf), "Trying %s", cfg);
        canvas_draw_str(canvas, 2, 25, buf);
        snprintf(buf, sizeof(buf), "%u of %u   %lu bytes", (unsigned)m->index + 1, (unsigned)baud_candidate_count,
                 (unsigned long)m->sample.bytes);
        canvas_draw_str(canvas, 2, 36, buf);
        uint32_t done = (uint32_t)(m->index * 124 / baud_candidate_count);
        canvas_draw_frame(canvas, 1, 42, 126, 6);
        canvas_draw_box(canvas, 2, 43, done, 4);
        canvas_draw_str(canvas, 2, 63, "Back: stop");
    } else {
        if(m->scores[m->best[0]] == 0) {
            canvas_draw_str(canvas, 2, 24, "Nothing readable arrived.");
            canvas_draw_str(canvas, 2, 33, "Is the device sending, and");
            canvas_draw_str(canvas, 2, 42, "TX/RX the right way round?");
        } else {
            for(uint8_t i = 0; i < 3; i++) {
                uint8_t c = m->best[i];
                label(&baud_candidates[c], cfg, sizeof(cfg));
                snprintf(buf, sizeof(buf), "%s  score %u%s", cfg, m->scores[c], m->scores[c] < 40 ? " unlikely" : "");
                canvas_draw_str(canvas, 2, 23 + 10 * i, buf);
            }
        }
        canvas_draw_str(canvas, 2, 63, m->status[0] ? m->status : "OK: use best  Back: exit");
    }
}

static void start_candidate(App* app, BaudModel* m) {
    serial_port_configure(app->port, &baud_candidates[m->index]);
    SerialErrors e;
    serial_port_errors(app->port, &e, true);
    memset(&m->sample, 0, sizeof(m->sample));
    m->window_start = furi_get_tick();
    if(m->probe) serial_port_write(app->port, (const uint8_t*)"\r", 1);
}

static void finish(App* app, BaudModel* m) {
    m->state = StateDone;
    // pick the three best scores; ties keep the earlier (more common) setting
    for(uint8_t k = 0; k < 3; k++) {
        int best = -1;
        for(size_t i = 0; i < baud_candidate_count; i++) {
            bool used = false;
            for(uint8_t j = 0; j < k; j++)
                if(m->best[j] == i) used = true;
            if(!used && (best < 0 || m->scores[i] > m->scores[best])) best = (int)i;
        }
        m->best[k] = (uint8_t)best;
    }
    serial_port_configure(app->port, &m->saved);
}

static void tick(App* app, void* context) {
    View* view = context;
    BaudModel* m = view_get_model(view);
    if(m->state != StateRunning) {
        view_commit_model(view, false);
        return;
    }
    SerialByte in[64];
    size_t n = serial_port_read(app->port, in, COUNT_OF(in));
    for(size_t i = 0; i < n; i++) {
        uint8_t b = in[i].byte;
        m->sample.bytes++;
        if((b >= 32 && b < 127) || b == '\r' || b == '\n' || b == '\t') m->sample.printable++;
    }
    if(furi_get_tick() - m->window_start >= WINDOW_MS) {
        SerialErrors e;
        serial_port_errors(app->port, &e, true);
        m->sample.frame_errors = e.frame;
        m->sample.parity_errors = e.parity;
        m->sample.noise_errors = e.noise;
        m->scores[m->index] = baud_score(&m->sample);
        m->bytes[m->index] = m->sample.bytes;
        m->index++;
        if(m->index >= baud_candidate_count)
            finish(app, m);
        else
            start_candidate(app, m);
    }
    view_commit_model(view, true);
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    View* view = app->baud_view;
    BaudModel* m = view_get_model(view);
    bool is_short = event->type == InputTypeShort;
    bool leave = false;
    if(event->key == InputKeyBack && is_short) {
        if(m->state == StateRunning) {
            m->state = StateIdle;
            serial_port_configure(app->port, &m->saved);
            snprintf(m->status, sizeof(m->status), "Stopped");
        } else {
            leave = true;
        }
    } else if(m->state == StateIdle) {
        if((event->key == InputKeyLeft || event->key == InputKeyRight) && is_short) m->probe = !m->probe;
        if(event->key == InputKeyOk && is_short) {
            m->state = StateRunning;
            m->index = 0;
            m->status[0] = 0;
            memset(m->scores, 0, sizeof(m->scores));
            start_candidate(app, m);
        }
    } else if(m->state == StateDone) {
        if(event->key == InputKeyOk && is_short && m->scores[m->best[0]] > 0) {
            SerialConfig best = baud_candidates[m->best[0]];
            m->saved = best;
            char cfg[24];
            label(&best, cfg, sizeof(cfg));
            snprintf(m->status, sizeof(m->status), "Now using %.20s", cfg);
            view_commit_model(view, true);
            app_apply_config(app, &best);
            return true;
        }
        if(event->key == InputKeyLeft && is_short) {
            m->state = StateIdle;
            m->status[0] = 0;
        }
    }
    view_commit_model(view, true);
    if(leave) app_switch(app, AppViewMenu);
    return true;
}

static void enter(void* context) {
    App* app = context;
    BaudModel* m = view_get_model(app->baud_view);
    m->state = StateIdle;
    m->probe = false; // the probe is opt-in for each run
    m->saved = app->config;
    m->status[0] = 0;
    view_commit_model(app->baud_view, true);
    app->tick_context = app->baud_view;
    app->tick = tick;
}

static void leave(void* context) {
    App* app = context;
    app->tick = NULL;
    BaudModel* m = view_get_model(app->baud_view);
    bool running = m->state == StateRunning;
    m->state = StateIdle;
    view_commit_model(app->baud_view, false);
    if(running) serial_port_configure(app->port, &app->config);
}

View* baud_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(BaudModel));
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    view_set_exit_callback(view, leave);
    return view;
}

void baud_view_free(View* view) {
    view_free(view);
}
