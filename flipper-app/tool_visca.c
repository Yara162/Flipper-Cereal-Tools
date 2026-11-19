// VISCA camera: client mode drives a camera; server mode acts as camera 1 so a control program
// can be tested. Uses 9600 8-N-1 while open and restores the previous settings on exit.
//
// Client controls. Hold OK switches mode. Pan/tilt: arrows move while held, OK = home.
// Zoom: Up/Down zoom while held. Focus: Left/Right near/far while held, OK = auto focus,
// Up = one-push focus. Presets: Left/Right pick 0-9, OK recalls, Down saves. Power: OK on/off,
// Up = address set, Down = ask power. Setup: Up/Down pick a row, Left/Right change it.
// The choose screen sets the baud rate (VISCA's default is 9600).
#include "tool_visca.h"
#include "core/visca.h"
#include "core/fmt.h"

typedef enum { ScreenChoose, ScreenClient, ScreenServer } Screen;
typedef enum { ModePanTilt, ModeZoom, ModeFocus, ModePresets, ModePower, ModeSetup, ModeCount } Mode;

static const uint32_t bauds[] = {9600, 19200, 38400, 115200};

#define SERVER_LINES 4

typedef struct {
    Screen screen;
    uint8_t choice;
    Mode mode;
    uint8_t address, pan_speed, tilt_speed, zoom_speed, preset, setup_row, baud_index;
    bool power_on_next;
    char last_sent[24];
    char last_reply[32];
    ViscaFramer framer;
    ViscaCamera camera;
    char server_lines[SERVER_LINES][40];
    char server_hex[SERVER_LINES][40];
    uint8_t server_count;
    bool server_hex_view;
    SerialConfig saved_config;
} ViscaModel;

static const char* mode_names[ModeCount] = {"Pan/tilt", "Zoom", "Focus", "Presets", "Power", "Setup"};

static void draw(Canvas* canvas, void* model) {
    ViscaModel* m = model;
    char buf[48];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    if(m->screen == ScreenChoose) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "VISCA camera");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 12, 127, 12);
        const char* rows[2] = {"Client: control a camera", "Server: act as a camera"};
        for(uint8_t i = 0; i < 2; i++) {
            int32_t y = 25 + 11 * i;
            if(i == m->choice) {
                canvas_draw_box(canvas, 0, y - 9, 128, 11);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 3, y, rows[i]);
            canvas_set_color(canvas, ColorBlack);
        }
        snprintf(buf, sizeof(buf), "Baud: %lu 8-N-1  <>", (unsigned long)bauds[m->baud_index]);
        canvas_draw_str(canvas, 2, 52, buf);
        canvas_draw_str(canvas, 2, 63, "OK: start");
        return;
    }
    if(m->screen == ScreenServer) {
        snprintf(
            buf, sizeof(buf), "Camera %u  %lu received", m->camera.address, (unsigned long)m->camera.received);
        canvas_draw_str(canvas, 1, 8, buf);
        canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, m->server_hex_view ? "HEX" : "");
        canvas_draw_line(canvas, 0, 10, 127, 10);
        if(!m->server_count) canvas_draw_str(canvas, 1, 22, "Waiting for commands...");
        for(uint8_t i = 0; i < m->server_count; i++)
            canvas_draw_str(canvas, 1, 19 + 9 * i, m->server_hex_view ? m->server_hex[i] : m->server_lines[i]);
        canvas_draw_line(canvas, 0, 54, 127, 54);
        snprintf(
            buf, sizeof(buf), "Power %s  Zoom %04X", m->camera.power ? "on" : "off", m->camera.zoom);
        canvas_draw_str(canvas, 1, 63, buf);
        return;
    }
    // client
    snprintf(buf, sizeof(buf), "Cam %u  %s", m->address, mode_names[m->mode]);
    canvas_draw_str(canvas, 1, 8, buf);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, "hold OK: mode");
    canvas_draw_line(canvas, 0, 10, 127, 10);
    switch(m->mode) {
    case ModePanTilt:
        snprintf(buf, sizeof(buf), "Speed pan %u tilt %u", m->pan_speed, m->tilt_speed);
        canvas_draw_str(canvas, 1, 21, "Arrows: move while held");
        canvas_draw_str(canvas, 1, 30, "OK: home");
        canvas_draw_str(canvas, 1, 39, buf);
        break;
    case ModeZoom:
        snprintf(buf, sizeof(buf), "Zoom speed %u (Setup)", m->zoom_speed);
        canvas_draw_str(canvas, 1, 21, "Up/Down: tele/wide while held");
        canvas_draw_str(canvas, 1, 30, buf);
        break;
    case ModeFocus:
        canvas_draw_str(canvas, 1, 21, "Left/Right: near/far, held");
        canvas_draw_str(canvas, 1, 30, "OK: auto focus");
        canvas_draw_str(canvas, 1, 39, "Up: one-push focus");
        break;
    case ModePresets:
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "< Preset %u >", m->preset);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, buf);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 1, 37, "OK: recall   Down: save");
        break;
    case ModePower:
        snprintf(buf, sizeof(buf), "OK: power %s", m->power_on_next ? "on" : "off");
        canvas_draw_str(canvas, 1, 21, buf);
        canvas_draw_str(canvas, 1, 30, "Up: address set");
        canvas_draw_str(canvas, 1, 39, "Down: ask power state");
        break;
    default: {
        const char* names[4] = {"Camera address", "Pan speed", "Tilt speed", "Zoom speed"};
        uint8_t values[4] = {m->address, m->pan_speed, m->tilt_speed, m->zoom_speed};
        for(uint8_t i = 0; i < 4; i++) {
            int32_t y = 19 + 9 * i;
            snprintf(buf, sizeof(buf), "%s: %u", names[i], values[i]);
            if(i == m->setup_row) {
                canvas_draw_box(canvas, 0, y - 8, 128, 9);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 1, y, buf);
            canvas_set_color(canvas, ColorBlack);
        }
        break;
    }
    }
    canvas_draw_line(canvas, 0, 45, 127, 45);
    snprintf(buf, sizeof(buf), "Sent %s", m->last_sent);
    canvas_draw_str(canvas, 1, 54, m->last_sent[0] ? buf : "");
    snprintf(buf, sizeof(buf), "Reply %s", m->last_reply);
    canvas_draw_str(canvas, 1, 63, m->last_reply[0] ? buf : "");
}

static void send(App* app, ViscaModel* m, ViscaCmd cmd, uint8_t arg1, uint8_t arg2, const char* label) {
    uint8_t pkt[VISCA_MAX_PACKET];
    size_t n = visca_build(cmd, m->address, arg1, arg2, pkt);
    serial_port_write(app->port, pkt, n);
    snprintf(m->last_sent, sizeof(m->last_sent), "%s", label);
    m->last_reply[0] = 0;
}

static void server_add(ViscaModel* m, const char* line, const uint8_t* pkt, uint8_t len) {
    if(m->server_count == SERVER_LINES) {
        memmove(m->server_lines[0], m->server_lines[1], sizeof(m->server_lines[0]) * (SERVER_LINES - 1));
        memmove(m->server_hex[0], m->server_hex[1], sizeof(m->server_hex[0]) * (SERVER_LINES - 1));
        m->server_count--;
    }
    snprintf(m->server_lines[m->server_count], sizeof(m->server_lines[0]), "%s", line);
    fmt_hex(pkt, len, m->server_hex[m->server_count], sizeof(m->server_hex[0]));
    m->server_count++;
}

static void tick(App* app, void* context) {
    View* view = context;
    SerialByte in[64];
    size_t n, total = 0;
    ViscaModel* m = view_get_model(view);
    while(total < SERIAL_DRAIN_MAX && (n = serial_port_read(app->port, in, COUNT_OF(in))) > 0) {
        total += n;
        for(size_t i = 0; i < n; i++) {
            uint8_t pkt[VISCA_MAX_PACKET], len;
            if(!visca_framer_feed(&m->framer, in[i].byte, pkt, &len)) continue;
            if(m->screen == ScreenClient) {
                visca_describe_reply(pkt, len, m->last_reply, sizeof(m->last_reply));
            } else if(m->screen == ScreenServer) {
                uint8_t reply[24];
                char desc[40];
                size_t r = visca_camera_handle(&m->camera, pkt, len, reply, sizeof(reply), desc, sizeof(desc));
                if(r) serial_port_write(app->port, reply, r);
                server_add(m, desc, pkt, len);
            }
        }
    }
    view_commit_model(view, total > 0);
}

static void client_input(App* app, ViscaModel* m, InputEvent* e) {
    bool press = e->type == InputTypePress, release = e->type == InputTypeRelease;
    bool is_short = e->type == InputTypeShort, step = is_short || e->type == InputTypeRepeat;
    InputKey k = e->key;
    switch(m->mode) {
    case ModePanTilt: {
        static const ViscaCmd dirs[] = {
            [InputKeyUp] = ViscaPanTiltUp,
            [InputKeyDown] = ViscaPanTiltDown,
            [InputKeyLeft] = ViscaPanTiltLeft,
            [InputKeyRight] = ViscaPanTiltRight,
        };
        if(k == InputKeyUp || k == InputKeyDown || k == InputKeyLeft || k == InputKeyRight) {
            if(press) send(app, m, dirs[k], m->pan_speed, m->tilt_speed, "move");
            if(release) send(app, m, ViscaPanTiltStop, m->pan_speed, m->tilt_speed, "stop");
        }
        if(k == InputKeyOk && is_short) send(app, m, ViscaPanTiltHome, 0, 0, "home");
        break;
    }
    case ModeZoom:
        if(k == InputKeyUp || k == InputKeyDown) {
            if(press)
                send(app, m, k == InputKeyUp ? ViscaZoomTele : ViscaZoomWide, m->zoom_speed, 0, k == InputKeyUp ? "zoom tele" : "zoom wide");
            if(release) send(app, m, ViscaZoomStop, 0, 0, "zoom stop");
        }
        break;
    case ModeFocus:
        if(k == InputKeyLeft || k == InputKeyRight) {
            if(press) {
                send(app, m, ViscaFocusManual, 0, 0, "focus manual");
                send(app, m, k == InputKeyLeft ? ViscaFocusNear : ViscaFocusFar, 0, 0, k == InputKeyLeft ? "focus near" : "focus far");
            }
            if(release) send(app, m, ViscaFocusStop, 0, 0, "focus stop");
        }
        if(k == InputKeyOk && is_short) send(app, m, ViscaFocusAuto, 0, 0, "auto focus");
        if(k == InputKeyUp && is_short) send(app, m, ViscaFocusOnePush, 0, 0, "one-push focus");
        break;
    case ModePresets:
        if(k == InputKeyLeft && step) m->preset = (uint8_t)((m->preset + 9) % 10);
        if(k == InputKeyRight && step) m->preset = (uint8_t)((m->preset + 1) % 10);
        if(k == InputKeyOk && is_short) {
            char label[24];
            snprintf(label, sizeof(label), "recall %u", m->preset);
            send(app, m, ViscaPresetRecall, m->preset, 0, label);
        }
        if(k == InputKeyDown && is_short) {
            char label[24];
            snprintf(label, sizeof(label), "save %u", m->preset);
            send(app, m, ViscaPresetSet, m->preset, 0, label);
        }
        break;
    case ModePower:
        if(k == InputKeyOk && is_short) {
            send(app, m, m->power_on_next ? ViscaPowerOn : ViscaPowerOff, 0, 0, m->power_on_next ? "power on" : "power off");
            m->power_on_next = !m->power_on_next;
        }
        if(k == InputKeyUp && is_short) send(app, m, ViscaAddressSet, 0, 0, "address set");
        if(k == InputKeyDown && is_short) send(app, m, ViscaInqPower, 0, 0, "ask power");
        break;
    default: {
        if(k == InputKeyUp && step) m->setup_row = (uint8_t)((m->setup_row + 3) % 4);
        if(k == InputKeyDown && step) m->setup_row = (uint8_t)((m->setup_row + 1) % 4);
        if((k == InputKeyLeft || k == InputKeyRight) && step) {
            int d = k == InputKeyRight ? 1 : -1;
            uint8_t* v[4] = {&m->address, &m->pan_speed, &m->tilt_speed, &m->zoom_speed};
            const uint8_t lo[4] = {1, 1, 1, 0}, hi[4] = {7, 24, 20, 7};
            int nv = *v[m->setup_row] + d;
            *v[m->setup_row] = (uint8_t)(nv < lo[m->setup_row] ? hi[m->setup_row] : nv > hi[m->setup_row] ? lo[m->setup_row] : nv);
        }
        break;
    }
    }
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    View* view = app->visca_view;
    ViscaModel* m = view_get_model(view);
    bool is_short = event->type == InputTypeShort;
    bool step = is_short || event->type == InputTypeRepeat;
    bool leave = false;
    if(event->key == InputKeyBack) {
        if(is_short) {
            if(m->screen == ScreenChoose)
                leave = true;
            else
                m->screen = ScreenChoose;
        }
    } else if(m->screen == ScreenChoose) {
        if((event->key == InputKeyUp || event->key == InputKeyDown) && step) m->choice ^= 1;
        if((event->key == InputKeyLeft || event->key == InputKeyRight) && is_short) {
            uint8_t count = (uint8_t)COUNT_OF(bauds);
            m->baud_index = (uint8_t)((m->baud_index + (event->key == InputKeyRight ? 1 : count - 1)) % count);
            const SerialConfig cfg = {.baud = bauds[m->baud_index], .data_bits = 8, .parity = 'N', .stop_bits = 1};
            serial_port_configure(app->port, &cfg);
        }
        if(event->key == InputKeyOk && is_short) {
            m->screen = m->choice == 0 ? ScreenClient : ScreenServer;
            memset(&m->framer, 0, sizeof(m->framer));
            serial_port_flush(app->port);
            if(m->screen == ScreenServer) {
                visca_camera_init(&m->camera);
                m->server_count = 0;
            }
        }
    } else if(m->screen == ScreenServer) {
        if(event->key == InputKeyOk && is_short) m->server_count = 0;
        if(event->key == InputKeyLeft && is_short) m->server_hex_view = !m->server_hex_view;
    } else {
        if(event->key == InputKeyOk && event->type == InputTypeLong)
            m->mode = (Mode)((m->mode + 1) % ModeCount);
        else
            client_input(app, m, event);
    }
    view_commit_model(view, true);
    if(leave) app_switch(app, AppViewMenu);
    return true;
}

static void enter(void* context) {
    App* app = context;
    ViscaModel* m = view_get_model(app->visca_view);
    m->saved_config = app->config;
    m->screen = ScreenChoose;
    if(!m->address) {
        m->address = 1;
        m->pan_speed = 8;
        m->tilt_speed = 5;
        m->zoom_speed = 4;
        m->power_on_next = true;
    }
    m->last_sent[0] = m->last_reply[0] = 0;
    uint32_t baud = bauds[m->baud_index];
    view_commit_model(app->visca_view, true);
    const SerialConfig visca = {.baud = baud, .data_bits = 8, .parity = 'N', .stop_bits = 1};
    if(app->port) serial_port_configure(app->port, &visca);
    app->tick_context = app->visca_view;
    app->tick = tick;
}

static void leave(void* context) {
    App* app = context;
    app->tick = NULL;
    ViscaModel* m = view_get_model(app->visca_view);
    SerialConfig saved = m->saved_config;
    view_commit_model(app->visca_view, false);
    if(app->port) serial_port_configure(app->port, &saved);
}

View* visca_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(ViscaModel));
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    view_set_exit_callback(view, leave);
    return view;
}

void visca_view_free(View* view) {
    view_free(view);
}
