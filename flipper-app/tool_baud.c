// Baud-rate finder: listens at each candidate setting and scores what arrives. A device that
// only answers when asked needs a probe, chosen per run (off by default). The cable check sends
// a test pattern with the far end's TX and RX joined, to prove the wiring.
#include "tool_baud.h"
#include "core/baudscore.h"
#include "core/fmt.h"

#define WINDOW_MS 1500
#define CABLE_WAIT_MS 400
#define CABLE_MAX 64

typedef enum { StateIdle, StateRunning, StateDone, StateCable, StateCableDone } State;
typedef enum { RowProbe, RowSearch, RowCable, RowCount } Row;

typedef struct {
    State state;
    Row row;
    uint8_t probe;
    size_t index;
    uint32_t window_start;
    BaudSample sample;
    uint8_t scores[16];
    uint8_t best[3];
    SerialConfig saved;
    uint8_t cable_rx[CABLE_MAX];
    size_t cable_len;
    char status[32];
} BaudModel;

static void draw_idle(Canvas* canvas, BaudModel* m) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Probe: %s", baud_probes[m->probe].name);
    const char* rows[RowCount] = {buf, "Find the baud rate", "Check cable (loopback)"};
    for(uint8_t i = 0; i < RowCount; i++) {
        int32_t y = 24 + 10 * i;
        if(i == m->row) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 3, y, rows[i]);
        if(i == RowProbe) canvas_draw_str_aligned(canvas, 125, y, AlignRight, AlignBottom, "<>");
        canvas_set_color(canvas, ColorBlack);
    }
    const char* hint = m->status[0]            ? m->status :
                       m->row == RowCable      ? "Join DB9 pins 2 and 3 first" :
                       m->probe == 0           ? "The device must be sending" :
                                                 "Sends the probe at each speed";
    canvas_draw_str(canvas, 2, 63, hint);
}

static void draw_cable(Canvas* canvas, BaudModel* m) {
    if(m->state == StateCable) {
        canvas_draw_str(canvas, 2, 26, "Sending the test pattern...");
        return;
    }
    size_t matched = 0;
    LoopbackVerdict v = loopback_judge(m->cable_rx, m->cable_len, &matched);
    char buf[48];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(
        canvas, 2, 25,
        v == LoopbackPass    ? "PASS" :
        v == LoopbackPartial ? "PARTLY" :
        v == LoopbackGarbled ? "GARBLED" :
                               "NOTHING CAME BACK");
    canvas_set_font(canvas, FontSecondary);
    if(v == LoopbackPass) {
        canvas_draw_str(canvas, 2, 35, "TX and RX reach the far end.");
    } else if(v == LoopbackNothing) {
        canvas_draw_str(canvas, 2, 35, "Check the jumper, ground,");
        canvas_draw_str(canvas, 2, 44, "and which pins are TX/RX.");
    } else {
        snprintf(buf, sizeof(buf), "%u of %u bytes came back", (unsigned)matched, (unsigned)loopback_pattern_len);
        canvas_draw_str(canvas, 2, 35, buf);
    }
    if(v != LoopbackNothing && v != LoopbackPass) {
        char rx[28];
        fmt_text(m->cable_rx, m->cable_len, rx, sizeof(rx));
        snprintf(buf, sizeof(buf), "RX %s", rx);
        canvas_draw_str(canvas, 2, 44, buf);
    }
    canvas_draw_str(canvas, 2, 63, "OK: check again  Back: done");
}

static void draw(Canvas* canvas, void* model) {
    BaudModel* m = model;
    char buf[48], cfg[24];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(
        canvas, 2, 10, (m->state == StateCable || m->state == StateCableDone) ? "Cable check" : "Baud-rate finder");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 12, 127, 12);
    if(m->state == StateIdle) {
        draw_idle(canvas, m);
    } else if(m->state == StateCable || m->state == StateCableDone) {
        draw_cable(canvas, m);
    } else if(m->state == StateRunning) {
        serial_config_label(&baud_candidates[m->index], cfg, sizeof(cfg));
        snprintf(buf, sizeof(buf), "Trying %s", cfg);
        canvas_draw_str(canvas, 2, 25, buf);
        snprintf(
            buf, sizeof(buf), "%u of %u   %lu bytes", (unsigned)m->index + 1, (unsigned)baud_candidate_count,
            (unsigned long)m->sample.bytes);
        canvas_draw_str(canvas, 2, 36, buf);
        uint32_t done = (uint32_t)(m->index * 124 / baud_candidate_count);
        canvas_draw_frame(canvas, 1, 42, 126, 6);
        canvas_draw_box(canvas, 2, 43, done, 4);
        canvas_draw_str(canvas, 2, 63, "Back: stop");
    } else {
        if(m->scores[m->best[0]] == 0) {
            canvas_draw_str(canvas, 2, 24, "Nothing readable arrived.");
            canvas_draw_str(canvas, 2, 33, m->probe ? "Try another probe, or the" : "Does it only answer when");
            canvas_draw_str(canvas, 2, 42, m->probe ? "cable check first." : "asked? Pick a probe.");
        } else {
            for(uint8_t i = 0; i < 3; i++) {
                uint8_t c = m->best[i];
                serial_config_label(&baud_candidates[c], cfg, sizeof(cfg));
                snprintf(buf, sizeof(buf), "%s  %u%s", cfg, m->scores[c], m->scores[c] < 40 ? " unlikely" : "%");
                canvas_draw_str(canvas, 2, 23 + 10 * i, buf);
            }
        }
        canvas_draw_str(canvas, 2, 63, m->status[0] ? m->status : "OK: use best  < again");
    }
}

static void start_candidate(App* app, BaudModel* m) {
    serial_port_configure(app->port, &baud_candidates[m->index]);
    SerialErrors e;
    serial_port_errors(app->port, &e, true);
    memset(&m->sample, 0, sizeof(m->sample));
    m->window_start = furi_get_tick();
    const BaudProbe* p = &baud_probes[m->probe];
    if(p->len) serial_port_write(app->port, p->data, p->len);
}

static void start_cable(App* app, BaudModel* m) {
    const SerialConfig cfg = {.baud = 9600, .data_bits = 8, .parity = 'N', .stop_bits = 1};
    serial_port_configure(app->port, &cfg); // also empties the queue
    m->cable_len = 0;
    m->state = StateCable;
    m->window_start = furi_get_tick();
    serial_port_write(app->port, loopback_pattern, loopback_pattern_len);
}

static void finish(App* app, BaudModel* m) {
    m->state = StateDone;
    // the three best scores; ties keep the earlier (more common) setting
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
    if(m->state != StateRunning && m->state != StateCable) {
        view_commit_model(view, false);
        return;
    }
    SerialByte in[64];
    size_t n, total = 0;
    while(total < SERIAL_DRAIN_MAX && (n = serial_port_read(app->port, in, COUNT_OF(in))) > 0) {
        total += n;
        for(size_t i = 0; i < n; i++) {
            uint8_t b = in[i].byte;
            if(m->state == StateCable) {
                if(m->cable_len < CABLE_MAX) m->cable_rx[m->cable_len++] = b;
                continue;
            }
            m->sample.bytes++;
            if((b >= 32 && b < 127) || b == '\r' || b == '\n' || b == '\t') m->sample.printable++;
        }
    }
    uint32_t elapsed = furi_get_tick() - m->window_start;
    if(m->state == StateCable) {
        if(elapsed >= CABLE_WAIT_MS) {
            m->state = StateCableDone;
            serial_port_configure(app->port, &m->saved);
        }
    } else if(elapsed >= WINDOW_MS) {
        SerialErrors e;
        serial_port_errors(app->port, &e, true);
        m->sample.frame_errors = e.frame;
        m->sample.parity_errors = e.parity;
        m->sample.noise_errors = e.noise;
        m->scores[m->index] = baud_score(&m->sample);
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
    bool step = is_short || event->type == InputTypeRepeat;
    bool leave = false;
    if(event->key == InputKeyBack && is_short) {
        if(m->state == StateRunning || m->state == StateCable) {
            serial_port_configure(app->port, &m->saved);
            snprintf(m->status, sizeof(m->status), "Stopped");
            m->state = StateIdle;
        } else if(m->state == StateIdle) {
            leave = true;
        } else {
            m->state = StateIdle;
            m->status[0] = 0;
        }
    } else if(m->state == StateIdle) {
        m->status[0] = 0;
        if(event->key == InputKeyUp && step) m->row = (Row)((m->row + RowCount - 1) % RowCount);
        if(event->key == InputKeyDown && step) m->row = (Row)((m->row + 1) % RowCount);
        if((event->key == InputKeyLeft || event->key == InputKeyRight) && step && m->row == RowProbe) {
            uint8_t count = (uint8_t)baud_probe_count;
            m->probe = (uint8_t)((m->probe + (event->key == InputKeyRight ? 1 : count - 1)) % count);
        }
        if(event->key == InputKeyOk && is_short) {
            if(m->row == RowCable) {
                start_cable(app, m);
            } else {
                m->state = StateRunning;
                m->index = 0;
                memset(m->scores, 0, sizeof(m->scores));
                start_candidate(app, m);
            }
        }
    } else if(m->state == StateCableDone) {
        if(event->key == InputKeyOk && is_short) start_cable(app, m);
    } else if(m->state == StateDone) {
        if(event->key == InputKeyOk && is_short && m->scores[m->best[0]] > 0) {
            SerialConfig best = baud_candidates[m->best[0]];
            m->saved = best;
            char cfg[24];
            serial_config_label(&best, cfg, sizeof(cfg));
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
    m->row = RowSearch;
    m->probe = 0; // the probe is opt-in for each visit
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
    bool busy = m->state == StateRunning || m->state == StateCable;
    m->state = StateIdle;
    view_commit_model(app->baud_view, false);
    if(busy) serial_port_configure(app->port, &app->config);
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
