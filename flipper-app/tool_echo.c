// Echo: shows what arrives; OK sends the latest message back exactly as received.
#include "tool_echo.h"
#include "msgring.h"
#include "log.h"
#include "core/fmt.h"

#define ROWS 4

typedef struct {
    MsgRing ring;
    MsgSplitter splitter;
    bool hex, auto_echo;
    uint32_t messages, bytes, echoed;
    SerialErrors errors;
    char status[32];
} EchoModel;

static void draw(Canvas* canvas, void* model) {
    EchoModel* m = model;
    char buf[40];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Echo  %lu msg  %lu B", (unsigned long)m->messages, (unsigned long)m->bytes);
    canvas_draw_str(canvas, 1, 8, buf);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, m->auto_echo ? "AUTO" : (m->hex ? "HEX" : ""));
    canvas_draw_line(canvas, 0, 10, 127, 10);
    if(!m->ring.count)
        canvas_draw_str(canvas, 1, 22, "Waiting for data...");
    else
        msgring_draw(canvas, &m->ring, 19, ROWS, m->hex, false, 0);
    canvas_draw_line(canvas, 0, 54, 127, 54);
    if(m->errors.frame || m->errors.parity || m->errors.dropped)
        snprintf(
            buf, sizeof(buf), "Errors: frame %lu parity %lu lost %lu", (unsigned long)m->errors.frame,
            (unsigned long)m->errors.parity, (unsigned long)m->errors.dropped);
    else if(m->status[0])
        snprintf(buf, sizeof(buf), "%s", m->status);
    else
        snprintf(buf, sizeof(buf), "OK echo  hold OK auto  < hex");
    canvas_draw_str(canvas, 1, 63, buf);
}

static void send_back(App* app, EchoModel* m, const Message* msg) {
    serial_port_write(app->port, msg->data, msg->len);
    m->echoed++;
    snprintf(m->status, sizeof(m->status), "Echoed %u bytes", msg->len);
}

static void tick(App* app, void* context) {
    View* view = context;
    SerialByte in[64];
    size_t n = serial_port_read(app->port, in, COUNT_OF(in));
    uint32_t now = furi_get_tick();
    EchoModel* m = view_get_model(view);
    bool changed = false;
    Message msg;
    for(size_t i = 0; i < n; i++) {
        m->bytes++;
        if(msgsplit_feed(&m->splitter, in[i].byte, in[i].at_ms, &msg)) {
            msgring_add(&m->ring, &msg);
            m->messages++;
            if(m->auto_echo) send_back(app, m, &msg);
            changed = true;
        }
    }
    if(msgsplit_poll(&m->splitter, now, &msg)) {
        msgring_add(&m->ring, &msg);
        m->messages++;
        if(m->auto_echo) send_back(app, m, &msg);
        changed = true;
    }
    SerialErrors e;
    serial_port_errors(app->port, &e, false);
    if(memcmp(&e, &m->errors, sizeof(e)) != 0) {
        m->errors = e;
        changed = true;
    }
    view_commit_model(view, changed || n > 0);
}

static void save(App* app, EchoModel* m) {
    char name[48];
    File* f = log_open("echo", &app->config, name, sizeof(name));
    if(!f) {
        snprintf(m->status, sizeof(m->status), "Could not save the log");
        return;
    }
    const size_t text_size = MSG_MAX * 4 + 8, hex_size = MSG_MAX * 3 + 8;
    char* text = malloc(text_size);
    char* hex = malloc(hex_size);
    for(uint8_t i = 0; i < m->ring.count; i++) {
        const Message* msg = msgring_get(&m->ring, i);
        fmt_text(msg->data, msg->len, text, text_size);
        fmt_hex(msg->data, msg->len, hex, hex_size);
        log_line(f, text);
        log_line(f, hex);
    }
    free(text);
    free(hex);
    log_close(f);
    snprintf(m->status, sizeof(m->status), "Saved %.24s", name);
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    View* view = app->echo_view;
    bool is_short = event->type == InputTypeShort;
    bool step = is_short || event->type == InputTypeRepeat;
    if(event->key == InputKeyBack && is_short) {
        app_switch(app, AppViewMenu);
        return true;
    }
    EchoModel* m = view_get_model(view);
    if(event->key == InputKeyOk && is_short) {
        const Message* last = msgring_newest(&m->ring);
        if(last)
            send_back(app, m, last);
        else
            snprintf(m->status, sizeof(m->status), "Nothing to echo yet");
    } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
        m->auto_echo = !m->auto_echo;
        snprintf(m->status, sizeof(m->status), m->auto_echo ? "Auto-echo on" : "Auto-echo off");
    } else if(event->key == InputKeyLeft && is_short) {
        m->hex = !m->hex;
    } else if(event->key == InputKeyRight && is_short) {
        save(app, m);
    } else if(event->key == InputKeyUp && step) {
        msgring_scroll(&m->ring, 1, ROWS);
    } else if(event->key == InputKeyDown && step) {
        msgring_scroll(&m->ring, -1, ROWS);
    }
    view_commit_model(view, true);
    return true;
}

static void enter(void* context) {
    App* app = context;
    EchoModel* m = view_get_model(app->echo_view);
    memset(m, 0, sizeof(*m));
    msgsplit_init(&m->splitter, msgsplit_gap_for_baud(app->config.baud));
    view_commit_model(app->echo_view, true);
    serial_port_flush(app->port);
    serial_port_errors(app->port, &(SerialErrors){0}, true);
    app->tick_context = app->echo_view;
    app->tick = tick;
}

static void leave(void* context) {
    App* app = context;
    app->tick = NULL;
}

View* echo_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EchoModel));
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    view_set_exit_callback(view, leave);
    return view;
}

void echo_view_free(View* view) {
    view_free(view);
}
