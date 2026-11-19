// Sniffer: listens only, never sends. Timestamps each message and can log to the SD card.
#include "tool_sniffer.h"
#include "msgring.h"
#include "log.h"
#include "core/fmt.h"

#define ROWS 4

typedef struct {
    MsgRing* ring; // only while the tool is open (Echo and Sniffer would hold 17 KB otherwise)
    MsgSplitter splitter;
    uint32_t start_ms;
    bool hex;
    uint32_t messages;
    SerialErrors errors;
    File* log;
    uint32_t logged;
    char status[32];
} SnifferModel;

static void draw(Canvas* canvas, void* model) {
    SnifferModel* m = model;
    char buf[48];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Sniffer  %lu msg", (unsigned long)m->messages);
    canvas_draw_str(canvas, 1, 8, buf);
    if(m->log)
        snprintf(buf, sizeof(buf), "LOG %lu", (unsigned long)m->logged);
    else
        snprintf(buf, sizeof(buf), "%s", m->hex ? "HEX" : "");
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 10, 127, 10);
    if(!m->ring || !m->ring->count)
        canvas_draw_str(canvas, 1, 22, "Listening...");
    else
        msgring_draw(canvas, m->ring, 19, ROWS, m->hex, true, m->start_ms);
    canvas_draw_line(canvas, 0, 54, 127, 54);
    if(m->status[0])
        snprintf(buf, sizeof(buf), "%s", m->status);
    else if(m->errors.frame || m->errors.parity || m->errors.dropped || m->errors.overrun)
        snprintf(
            buf, sizeof(buf), "Frame %lu parity %lu lost %lu", (unsigned long)m->errors.frame,
            (unsigned long)m->errors.parity, (unsigned long)(m->errors.dropped + m->errors.overrun));
    else
        snprintf(buf, sizeof(buf), "OK log  < hex  > settings");
    canvas_draw_str(canvas, 1, 63, buf);
}

static void log_message(SnifferModel* m, const Message* msg) {
    if(!m->log) return;
    char* line = malloc(MSG_MAX * 7 + 32);
    uint32_t t = msg->at_ms - m->start_ms;
    int n = snprintf(
        line, 32, "%02lu:%02lu.%03lu  ", (unsigned long)(t / 60000 % 100), (unsigned long)(t / 1000 % 60),
        (unsigned long)(t % 1000));
    n += (int)fmt_hex(msg->data, msg->len, line + n, MSG_MAX * 3 + 8);
    n += snprintf(line + n, 8, "  |  ");
    fmt_text(msg->data, msg->len, line + n, MSG_MAX * 4 + 8);
    if(msg->truncated) strlcat(line, "  (continues)", MSG_MAX * 7 + 32);
    log_line(m->log, line);
    free(line);
    m->logged++;
}

static void add(SnifferModel* m, const Message* msg) {
    msgring_add(m->ring, msg);
    m->messages++;
    log_message(m, msg);
}

static void tick(App* app, void* context) {
    View* view = context;
    SerialByte in[64];
    size_t n, total = 0;
    SnifferModel* m = view_get_model(view);
    Message msg;
    while(total < SERIAL_DRAIN_MAX && (n = serial_port_read(app->port, in, COUNT_OF(in))) > 0) {
        total += n;
        for(size_t i = 0; i < n; i++)
            if(msgsplit_feed(&m->splitter, in[i].byte, in[i].at_ms, &msg)) add(m, &msg);
    }
    bool changed = total > 0;
    // only judge a quiet gap once the queue is empty, or a backlog looks like silence
    if(total < SERIAL_DRAIN_MAX && msgsplit_poll(&m->splitter, furi_get_tick(), &msg)) {
        add(m, &msg);
        changed = true;
    }
    SerialErrors e;
    serial_port_errors(app->port, &e, false);
    if(memcmp(&e, &m->errors, sizeof(e)) != 0) {
        m->errors = e;
        changed = true;
    }
    view_commit_model(view, changed);
}

static void toggle_log(App* app, SnifferModel* m) {
    if(m->log) {
        log_close(m->log);
        m->log = NULL;
        snprintf(m->status, sizeof(m->status), "Log saved, %lu lines", (unsigned long)m->logged);
        return;
    }
    char name[48];
    m->log = log_open("sniffer", &app->config, name, sizeof(name));
    m->logged = 0;
    snprintf(m->status, sizeof(m->status), m->log ? "Logging to SD card" : "Could not start the log");
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    View* view = app->sniffer_view;
    bool is_short = event->type == InputTypeShort;
    bool step = is_short || event->type == InputTypeRepeat;
    if(event->key == InputKeyBack && is_short) {
        app_switch(app, AppViewMenu);
        return true;
    }
    if(event->key == InputKeyRight && is_short) {
        app_open_settings(app, AppViewSniffer);
        return true;
    }
    SnifferModel* m = view_get_model(view);
    if(event->key == InputKeyOk && is_short) toggle_log(app, m);
    if(event->key == InputKeyLeft && is_short) m->hex = !m->hex;
    if(event->key == InputKeyUp && step) msgring_scroll(m->ring, 1, ROWS);
    if(event->key == InputKeyDown && step) msgring_scroll(m->ring, -1, ROWS);
    if(event->key != InputKeyOk) m->status[0] = 0;
    view_commit_model(view, true);
    return true;
}

static void enter(void* context) {
    App* app = context;
    SnifferModel* m = view_get_model(app->sniffer_view);
    // Returning from the settings screen keeps the list; a fresh visit starts over
    if((!m->log && app->settings_back != AppViewSniffer) || !m->ring) {
        free(m->ring);
        File* log = m->log;
        memset(m, 0, sizeof(*m));
        m->log = log;
        m->ring = malloc(sizeof(MsgRing));
        memset(m->ring, 0, sizeof(MsgRing));
        m->start_ms = furi_get_tick();
    }
    app->settings_back = AppViewMenu;
    msgsplit_init(&m->splitter, msgsplit_gap_for_baud(app->config.baud));
    view_commit_model(app->sniffer_view, true);
    serial_port_errors(app->port, &(SerialErrors){0}, true);
    app->tick_context = app->sniffer_view;
    app->tick = tick;
}

static void leave(void* context) {
    App* app = context;
    app->tick = NULL;
    if(app->settings_back == AppViewSniffer) return; // just visiting settings: keep logging
    SnifferModel* m = view_get_model(app->sniffer_view);
    if(m->log) {
        log_close(m->log);
        m->log = NULL;
    }
    free(m->ring);
    m->ring = NULL;
    view_commit_model(app->sniffer_view, false);
}

View* sniffer_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(SnifferModel));
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    view_set_exit_callback(view, leave);
    return view;
}

void sniffer_view_free(View* view) {
    SnifferModel* m = view_get_model(view);
    free(m->ring);
    m->ring = NULL;
    view_commit_model(view, false);
    view_free(view);
}
