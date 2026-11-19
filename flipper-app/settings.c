#include "settings.h"
#include <storage/storage.h>
#include <stdlib.h>

#define SETTINGS_FILE APP_DATA "/settings.txt"

static const uint32_t bauds[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
#define N_BAUDS COUNT_OF(bauds)

void serial_config_label(const SerialConfig* cfg, char* out, size_t size) {
    snprintf(
        out, size, "%lu %u-%c-%u", (unsigned long)cfg->baud, cfg->data_bits, cfg->parity, cfg->stop_bits);
}

static void defaults(SerialConfig* cfg) {
    cfg->baud = 9600;
    cfg->data_bits = 8;
    cfg->parity = 'N';
    cfg->stop_bits = 1;
}

void settings_load(SerialConfig* cfg) {
    defaults(cfg);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char buf[128] = {0};
    if(storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING))
        storage_file_read(file, buf, sizeof(buf) - 1);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    char* next = buf;
    while(next && *next) {
        char* line = next;
        char* nl = strchr(line, '\n');
        if(nl) *nl = 0;
        next = nl ? nl + 1 : NULL;
        char* eq = strchr(line, '=');
        if(!eq) continue;
        *eq = 0;
        const char* v = eq + 1;
        if(strcmp(line, "baud") == 0) {
            long b = strtol(v, NULL, 10);
            if(b >= 300 && b <= 1000000) cfg->baud = (uint32_t)b;
        } else if(strcmp(line, "data_bits") == 0 && (v[0] == '7' || v[0] == '8')) {
            cfg->data_bits = (uint8_t)(v[0] - '0');
        } else if(strcmp(line, "parity") == 0 && (v[0] == 'N' || v[0] == 'E' || v[0] == 'O')) {
            cfg->parity = v[0];
        } else if(strcmp(line, "stop_bits") == 0 && (v[0] == '1' || v[0] == '2')) {
            cfg->stop_bits = (uint8_t)(v[0] - '0');
        }
    }
}

void settings_save(const SerialConfig* cfg) {
    char buf[96];
    int n = snprintf(
        buf, sizeof(buf), "baud=%lu\ndata_bits=%u\nparity=%c\nstop_bits=%u\n", (unsigned long)cfg->baud,
        cfg->data_bits, cfg->parity, cfg->stop_bits);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, buf, (size_t)n);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- settings screen ---

typedef struct {
    SerialConfig cfg;
    uint8_t row;
} SettingsModel;

static void draw(Canvas* canvas, void* model) {
    SettingsModel* m = model;
    char buf[32];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Serial settings");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 12, 127, 12);
    const char* names[] = {"Baud rate", "Data bits", "Parity", "Stop bits"};
    for(uint8_t i = 0; i < 4; i++) {
        int32_t y = 23 + 10 * i;
        if(i == 0)
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->cfg.baud);
        else if(i == 1)
            snprintf(buf, sizeof(buf), "%u", m->cfg.data_bits);
        else if(i == 2)
            snprintf(
                buf, sizeof(buf), "%s", m->cfg.parity == 'E' ? "Even" : m->cfg.parity == 'O' ? "Odd" : "None");
        else
            snprintf(buf, sizeof(buf), "%u", m->cfg.stop_bits);
        if(i == m->row) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, names[i]);
        canvas_draw_str_aligned(canvas, 110, y, AlignRight, AlignBottom, buf);
        if(i == m->row) {
            canvas_draw_str(canvas, 114, y, "<>");
            canvas_set_color(canvas, ColorBlack);
        }
    }
    canvas_draw_str(canvas, 2, 63, "Back: save and return");
}

static void step(SerialConfig* cfg, uint8_t row, int dir) {
    if(row == 0) {
        size_t i = 0;
        while(i < N_BAUDS && bauds[i] != cfg->baud) i++;
        if(i == N_BAUDS) i = 3; // unusual rate from a file: start from 9600
        i = (i + N_BAUDS + (size_t)(dir > 0 ? 1 : N_BAUDS - 1)) % N_BAUDS;
        cfg->baud = bauds[i];
    } else if(row == 1) {
        cfg->data_bits = cfg->data_bits == 8 ? 7 : 8;
    } else if(row == 2) {
        const char order[] = {'N', 'E', 'O'};
        int i = cfg->parity == 'E' ? 1 : cfg->parity == 'O' ? 2 : 0;
        cfg->parity = order[(i + 3 + dir) % 3];
    } else {
        cfg->stop_bits = cfg->stop_bits == 1 ? 2 : 1;
    }
}

static bool input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) {
        SerialConfig cfg;
        with_view_model(app->settings_view, SettingsModel * m, { cfg = m->cfg; }, false);
        app_apply_config(app, &cfg);
        app_switch(app, app->settings_back);
        return true;
    }
    with_view_model(
        app->settings_view,
        SettingsModel * m,
        {
            if(event->key == InputKeyUp) m->row = (uint8_t)((m->row + 3) % 4);
            if(event->key == InputKeyDown) m->row = (uint8_t)((m->row + 1) % 4);
            if(event->key == InputKeyLeft) step(&m->cfg, m->row, -1);
            if(event->key == InputKeyRight || event->key == InputKeyOk) step(&m->cfg, m->row, 1);
        },
        true);
    return true;
}

static void enter(void* context) {
    App* app = context;
    with_view_model(app->settings_view, SettingsModel * m, { m->cfg = app->config; }, true);
}

View* settings_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(SettingsModel));
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_enter_callback(view, enter);
    return view;
}

void settings_view_free(View* view) {
    view_free(view);
}
