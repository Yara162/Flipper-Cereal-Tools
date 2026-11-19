// State shared by the app and its tools.
#pragma once
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include "serial_port.h"

#define APP_DATA STORAGE_APP_DATA_PATH_PREFIX
#define APP_ASSETS STORAGE_APP_ASSETS_PATH_PREFIX

typedef enum {
    AppViewMenu,
    AppViewSettings,
    AppViewMessage,
    AppViewList, // shared picker for Display, Projector and Custom commands
    AppViewEcho,
    AppViewSniffer,
    AppViewCmdSet,
    AppViewVisca,
    AppViewBaud,
} AppView;

typedef struct App App;
typedef void (*AppTickCallback)(App* app, void* context);

struct App {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* menu;
    Submenu* list;
    View* settings_view;
    View* message_view;
    View* echo_view;
    View* sniffer_view;
    View* cmdset_view;
    View* visca_view;
    View* baud_view;
    SerialPort* port;
    SerialConfig config;
    char menu_header[40];
    uint32_t settings_back; // view Back returns to from the settings screen
    uint32_t message_back;
    uint32_t current_view;
    AppTickCallback tick; // called every 50 ms on the GUI thread while set
    void* tick_context;
    void* tool; // state owned by the active list tool (display, projector, custom)
};

void app_switch(App* app, uint32_t view);
// Applies serial settings to the port and saves them.
void app_apply_config(App* app, const SerialConfig* cfg);
// Opens the settings screen; Back returns to `back`.
void app_open_settings(App* app, uint32_t back);
// Shows up to four lines of text; Back returns to `back`.
void app_show_message(App* app, const char* l1, const char* l2, const char* l3, const char* l4, uint32_t back);

// "9600 8-N-1"
void serial_config_label(const SerialConfig* cfg, char* out, size_t size);
