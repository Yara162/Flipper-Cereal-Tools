#include "cmdset_view.h"
#include "log.h"
#include "core/render.h"
#include "core/fmt.h"
#include <storage/storage.h>

#define FILE_MAX 8192
#define MAX_ROWS (CMDSET_MAX_SETTINGS + CMDSET_MAX_COMMANDS * 2)
#define LIST_LINES 5
#define TX_MAX 128
#define RX_MAX 256
#define REPLY_TIMEOUT_MS 1000
#define REPLY_QUIET_MS 150
#define HISTORY 32

typedef enum { RowSetting, RowHeader, RowCommand } RowKind;

typedef struct {
    RowKind kind;
    uint8_t index; // setting or command index; for headers, the command whose group starts here
} Row;

typedef enum { ModeList, ModeWaiting, ModeResult } Mode;

typedef struct {
    char name[32];
    char tx[49];
    char rx[81];
    char verdict[12];
} HistoryEntry;

typedef struct {
    CommandSet* set;
    char path[96];
    Row rows[MAX_ROWS];
    uint8_t row_count, cursor, top;
    Mode mode;
    bool hex_rx;
    uint8_t command; // command being sent or shown
    uint8_t tx[TX_MAX];
    int tx_len;
    uint8_t rx[RX_MAX];
    size_t rx_len;
    uint32_t sent_at, last_rx_at;
    uint8_t expect[96], expect_mask[96];
    int expect_len; // -1: command has no expected reply
    bool matched;
    char error[48];
    char status[40];
    HistoryEntry history[HISTORY];
    uint8_t history_count;
} CmdSetModel;

static uint32_t cmdset_back = AppViewMenu;

// --- list helpers ---

static void build_rows(CmdSetModel* m) {
    m->row_count = 0;
    for(uint8_t i = 0; i < m->set->setting_count; i++) m->rows[m->row_count++] = (Row){RowSetting, i};
    const char* group = NULL;
    for(uint8_t i = 0; i < m->set->command_count; i++) {
        const char* g = m->set->commands[i].group;
        if(g[0] && (!group || strcmp(g, group) != 0)) {
            m->rows[m->row_count++] = (Row){RowHeader, i};
            group = g;
        }
        m->rows[m->row_count++] = (Row){RowCommand, i};
    }
    m->cursor = 0;
    while(m->cursor < m->row_count && m->rows[m->cursor].kind == RowHeader) m->cursor++;
    m->top = 0;
}

static void move_cursor(CmdSetModel* m, int dir) {
    int c = m->cursor;
    do {
        c += dir;
        if(c < 0 || c >= m->row_count) return;
    } while(m->rows[c].kind == RowHeader);
    m->cursor = (uint8_t)c;
    if(m->cursor < m->top) m->top = m->cursor;
    // show the group header above the first command when scrolling up to it
    if(m->top > 0 && m->cursor == m->top && m->rows[m->top - 1].kind == RowHeader) m->top--;
    if(m->cursor >= m->top + LIST_LINES) m->top = (uint8_t)(m->cursor - LIST_LINES + 1);
}

// --- drawing ---

static void draw_list(Canvas* canvas, CmdSetModel* m) {
    char buf[48];
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%s%s", m->set->name, m->set->tested ? "" : " *");
    canvas_draw_str(canvas, 1, 8, buf);
    canvas_draw_line(canvas, 0, 10, 127, 10);
    for(uint8_t i = 0; i < LIST_LINES && m->top + i < m->row_count; i++) {
        const Row* r = &m->rows[m->top + i];
        int32_t y = 19 + 9 * i;
        bool selected = m->top + i == m->cursor;
        if(r->kind == RowHeader) {
            canvas_draw_str(canvas, 1, y, m->set->commands[r->index].group);
            canvas_draw_line(canvas, 0, y + 1, 127, y + 1);
            continue;
        }
        if(r->kind == RowSetting) {
            const CmdSetting* s = &m->set->settings[r->index];
            snprintf(buf, sizeof(buf), "%s: %ld  <>", s->label, (long)s->value);
        } else {
            snprintf(buf, sizeof(buf), "  %s", m->set->commands[r->index].name);
        }
        if(selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 1, y, buf);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 55, 127, 55);
    canvas_draw_str(canvas, 1, 63, m->status[0] ? m->status : "OK send  hold OK settings");
}

static void draw_result(Canvas* canvas, CmdSetModel* m) {
    char buf[64];
    const Command* c = &m->set->commands[m->command];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, c->name);
    const char* verdict = m->mode == ModeWaiting ? "..." :
                          m->error[0]            ? "ERROR" :
                          m->expect_len < 0      ? (m->rx_len ? "" : "NO REPLY") :
                          m->matched             ? "OK" :
                          m->rx_len              ? "UNEXPECTED" :
                                                   "NO REPLY";
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, verdict);
    canvas_draw_line(canvas, 0, 10, 127, 10);
    if(m->error[0]) {
        canvas_draw_str(canvas, 1, 22, "Not sent:");
        canvas_draw_str(canvas, 1, 32, m->error);
        return;
    }
    char hex[64]; // only the start fits on screen
    fmt_hex(m->tx, (size_t)m->tx_len, hex, sizeof(hex));
    snprintf(buf, sizeof(buf), "TX %.58s", hex);
    canvas_draw_str(canvas, 1, 19, buf);
    char rx[64]; // draw code runs on the GUI's small stack; only ~44 characters are shown
    if(m->hex_rx)
        fmt_hex(m->rx, m->rx_len, rx, sizeof(rx));
    else
        fmt_text(m->rx, m->rx_len, rx, sizeof(rx));
    // reply over two lines of about 24 characters
    char line[26];
    size_t len = strlen(rx);
    snprintf(line, sizeof(line), "RX %.22s", rx);
    canvas_draw_str(canvas, 1, 28, m->rx_len ? line : "RX (nothing yet)");
    if(len > 22) {
        snprintf(line, sizeof(line), "   %.22s", rx + 22);
        canvas_draw_str(canvas, 1, 37, line);
    }
    // values read out of the reply
    int32_t y = 46;
    for(uint8_t i = 0; i < m->set->show_count && y <= 54; i++) {
        const ShowRule* r = &m->set->shows[i];
        if(r->command != m->command) continue;
        char value[32];
        if(m->rx_len && render_show(r, m->rx, m->rx_len, value, sizeof(value)))
            snprintf(buf, sizeof(buf), "%s: %s", r->label, value);
        else
            snprintf(buf, sizeof(buf), "%s: ?", r->label);
        canvas_draw_str(canvas, 1, y, buf);
        y += 9;
    }
    canvas_draw_str(canvas, 1, 63, m->status[0] ? m->status : "OK again  < hex/text");
}

static void draw(Canvas* canvas, void* model) {
    CmdSetModel* m = model;
    canvas_clear(canvas);
    if(!m->set) return;
    if(m->mode == ModeList)
        draw_list(canvas, m);
    else
        draw_result(canvas, m);
}

// --- sending ---

static void remember(CmdSetModel* m) {
    if(m->history_count == HISTORY) {
        memmove(&m->history[0], &m->history[1], sizeof(HistoryEntry) * (HISTORY - 1));
        m->history_count--;
    }
    HistoryEntry* h = &m->history[m->history_count++];
    snprintf(h->name, sizeof(h->name), "%s", m->set->commands[m->command].name);
    fmt_hex(m->tx, (size_t)(m->tx_len > 0 ? m->tx_len : 0), h->tx, sizeof(h->tx));
    fmt_text(m->rx, m->rx_len, h->rx, sizeof(h->rx));
    snprintf(
        h->verdict, sizeof(h->verdict), "%s",
        m->error[0]       ? "error" :
        m->expect_len < 0 ? (m->rx_len ? "reply" : "no reply") :
        m->matched        ? "OK" :
        m->rx_len         ? "unexpected" :
                            "no reply");
}

static void send(App* app, CmdSetModel* m, uint8_t index) {
    const Command* c = &m->set->commands[index];
    m->command = index;
    m->mode = ModeWaiting;
    m->rx_len = 0;
    m->matched = false;
    m->error[0] = 0;
    m->status[0] = 0;
    m->tx_len = render_bytes(c->data, c->kind, m->set, m->tx, sizeof(m->tx), m->error, sizeof(m->error));
    m->expect_len = -1;
    if(m->tx_len >= 0 && c->has_expect) {
        m->expect_len = render_pattern(
            c->expect, c->expect_kind, m->set, m->expect, m->expect_mask, sizeof(m->expect), m->error,
            sizeof(m->error));
        if(m->expect_len < 0) m->tx_len = -1; // a broken expected reply: do not send
    }
    if(m->tx_len < 0) {
        m->mode = ModeResult;
        remember(m);
        return;
    }
    serial_port_flush(app->port);
    serial_port_write(app->port, m->tx, (size_t)m->tx_len);
    m->sent_at = m->last_rx_at = furi_get_tick();
}

static void tick(App* app, void* context) {
    View* view = context;
    CmdSetModel* m = view_get_model(view);
    if(m->mode != ModeWaiting) {
        view_commit_model(view, false);
        return;
    }
    SerialByte in[64];
    size_t n = serial_port_read(app->port, in, COUNT_OF(in));
    for(size_t i = 0; i < n && m->rx_len < RX_MAX; i++) m->rx[m->rx_len++] = in[i].byte;
    uint32_t now = furi_get_tick();
    if(n) m->last_rx_at = now;
    if(m->expect_len >= 0 && render_match(m->rx, m->rx_len, m->expect, m->expect_mask, (size_t)m->expect_len))
        m->matched = true;
    bool done = m->matched || now - m->sent_at >= REPLY_TIMEOUT_MS ||
                (m->rx_len && now - m->last_rx_at >= REPLY_QUIET_MS) || m->rx_len >= RX_MAX;
    if(done) {
        m->mode = ModeResult;
        remember(m);
    }
    view_commit_model(view, n > 0 || done);
}

static void save(App* app, CmdSetModel* m) {
    char name[48];
    File* f = log_open("commands", &app->config, name, sizeof(name));
    if(!f) {
        snprintf(m->status, sizeof(m->status), "Could not save the log");
        return;
    }
    char line[200];
    snprintf(line, sizeof(line), "Command set: %s (%s)", m->set->name, m->path);
    log_line(f, line);
    for(uint8_t i = 0; i < m->history_count; i++) {
        const HistoryEntry* h = &m->history[i];
        snprintf(line, sizeof(line), "%s [%s]\n  TX %s\n  RX %s", h->name, h->verdict, h->tx, h->rx);
        log_line(f, line);
    }
    log_close(f);
    snprintf(m->status, sizeof(m->status), "Saved %.30s", name);
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    View* view = app->cmdset_view;
    bool is_short = event->type == InputTypeShort, is_long = event->type == InputTypeLong;
    bool step = is_short || event->type == InputTypeRepeat;
    CmdSetModel* m = view_get_model(view);
    bool open_settings = false, go_back = false;
    if(m->mode == ModeList) {
        const Row* r = m->row_count ? &m->rows[m->cursor] : NULL;
        if(event->key == InputKeyBack && is_short) go_back = true;
        if(event->key == InputKeyUp && step) move_cursor(m, -1);
        if(event->key == InputKeyDown && step) move_cursor(m, 1);
        if(r && r->kind == RowSetting && step && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            CmdSetting* s = &m->set->settings[r->index];
            int32_t v = s->value + (event->key == InputKeyRight ? 1 : -1);
            s->value = v < s->min ? s->max : v > s->max ? s->min : v;
        }
        if(event->key == InputKeyOk && is_short && r && r->kind == RowCommand) send(app, m, r->index);
        if(event->key == InputKeyOk && is_long) open_settings = true;
        if(event->key == InputKeyRight && is_long) save(app, m);
        if(event->key != InputKeyRight) m->status[0] = 0;
    } else if(m->mode == ModeResult) {
        if(event->key == InputKeyBack && is_short) {
            m->mode = ModeList;
            m->status[0] = 0;
        }
        if(event->key == InputKeyOk && is_short) send(app, m, m->command);
        if(event->key == InputKeyLeft && is_short) m->hex_rx = !m->hex_rx;
    }
    view_commit_model(view, true);
    if(open_settings) app_open_settings(app, AppViewCmdSet);
    if(go_back) app_switch(app, cmdset_back);
    return true;
}

static void enter(void* context) {
    App* app = context;
    app->tick_context = app->cmdset_view;
    app->tick = tick;
}

static void leave(void* context) {
    App* app = context;
    app->tick = NULL;
}

void cmdset_view_open(App* app, const char* path, uint32_t back) {
    cmdset_back = back;
    CmdSetModel* m = view_get_model(app->cmdset_view);
    char* buf = malloc(FILE_MAX);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    size_t len = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) len = storage_file_read(f, buf, FILE_MAX - 1);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    buf[len] = 0;
    CmdFileError err;
    bool ok = cmdfile_parse(buf, len, m->set, &err);
    free(buf);
    if(!ok) {
        view_commit_model(app->cmdset_view, false);
        char line[24];
        snprintf(line, sizeof(line), "Line %d:", err.line);
        app_show_message(app, "Could not load file", line, err.message, "", back);
        return;
    }
    snprintf(m->path, sizeof(m->path), "%s", path);
    m->mode = ModeList;
    m->history_count = 0;
    m->status[0] = 0;
    build_rows(m);
    SerialConfig cfg = m->set->serial;
    bool apply = m->set->has_serial;
    if(apply) {
        char label[24];
        serial_config_label(&cfg, label, sizeof(label));
        snprintf(m->status, sizeof(m->status), "From file: %.24s", label);
    }
    view_commit_model(app->cmdset_view, true);
    if(apply) app_apply_config(app, &cfg);
    app_switch(app, AppViewCmdSet);
}

View* cmdset_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(CmdSetModel));
    CmdSetModel* m = view_get_model(view);
    m->set = malloc(sizeof(CommandSet));
    memset(m->set, 0, sizeof(CommandSet));
    view_commit_model(view, false);
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    view_set_exit_callback(view, leave);
    return view;
}

void cmdset_view_free(View* view) {
    CmdSetModel* m = view_get_model(view);
    free(m->set);
    view_commit_model(view, false);
    view_free(view);
}
