// Cereal Tools: RS-232 tools for AV commissioning with the Rabbit-Labs Flip'n Cereal HAT.
#include "app.h"
#include "settings.h"
#include "tool_echo.h"
#include "tool_sniffer.h"
#include "tool_custom.h"
#include "tool_devices.h"
#include "tool_visca.h"
#include "cmdset_view.h"
#include "catalog.h"

typedef enum {
    MenuDisplay,
    MenuProjector,
    MenuCustom,
    MenuEcho,
    MenuSniffer,
    MenuVisca,
    MenuBaud,
    MenuSettings,
} MenuItem;

typedef struct {
    char lines[4][40];
} MessageModel;

void app_switch(App* app, uint32_t view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void update_menu_header(App* app) {
    char label[24];
    serial_config_label(&app->config, label, sizeof(label));
    snprintf(app->menu_header, sizeof(app->menu_header), "Cereal Tools  %s", label);
    submenu_set_header(app->menu, app->menu_header);
}

void app_apply_config(App* app, const SerialConfig* cfg) {
    app->config = *cfg;
    if(app->port) serial_port_configure(app->port, cfg);
    settings_save(cfg);
    update_menu_header(app);
}

void app_open_settings(App* app, uint32_t back) {
    app->settings_back = back;
    app_switch(app, AppViewSettings);
}

void app_show_message(App* app, const char* l1, const char* l2, const char* l3, const char* l4, uint32_t back) {
    app->message_back = back;
    const char* src[4] = {l1, l2, l3, l4};
    MessageModel* m = view_get_model(app->message_view);
    for(int i = 0; i < 4; i++) snprintf(m->lines[i], sizeof(m->lines[i]), "%s", src[i] ? src[i] : "");
    view_commit_model(app->message_view, true);
    app_switch(app, AppViewMessage);
}

static void message_draw(Canvas* canvas, void* model) {
    MessageModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, m->lines[0]);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 12, 127, 12);
    for(int i = 1; i < 4; i++) canvas_draw_str(canvas, 2, 12 + 11 * i, m->lines[i]);
    canvas_draw_str(canvas, 2, 63, "Back: return");
}

static bool message_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        app_switch(app, app->message_back);
        return true;
    }
    return false;
}

static void menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MenuSettings:
        app_open_settings(app, AppViewMenu);
        break;
    case MenuEcho:
        app_switch(app, AppViewEcho);
        break;
    case MenuDisplay:
        display_open(app);
        break;
    case MenuProjector:
        projector_open(app);
        break;
    case MenuCustom:
        custom_open(app);
        break;
    case MenuVisca:
        app_switch(app, AppViewVisca);
        break;
    case MenuSniffer:
        app->settings_back = AppViewMenu;
        app_switch(app, AppViewSniffer);
        break;
    default:
        app_show_message(app, "Not built yet", "This tool arrives in a", "later step of the build.", "", AppViewMenu);
        break;
    }
}

static uint32_t exit_app(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void tick_callback(void* context) {
    App* app = context;
    if(app->tick) app->tick(app, app->tick_context);
}

static bool back_event(void* context) {
    UNUSED(context);
    return false; // views handle Back themselves; the menu's previous callback exits
}

int32_t cereal_tools_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    settings_load(&app->config);
    app->port = serial_port_open(&app->config);

    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick_callback, furi_ms_to_ticks(50));
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back_event);

    app->menu = submenu_alloc();
    submenu_add_item(app->menu, "Display control", MenuDisplay, menu_callback, app);
    submenu_add_item(app->menu, "Projector control", MenuProjector, menu_callback, app);
    submenu_add_item(app->menu, "Custom commands", MenuCustom, menu_callback, app);
    submenu_add_item(app->menu, "Echo", MenuEcho, menu_callback, app);
    submenu_add_item(app->menu, "Sniffer", MenuSniffer, menu_callback, app);
    submenu_add_item(app->menu, "VISCA camera", MenuVisca, menu_callback, app);
    submenu_add_item(app->menu, "Baud-rate finder", MenuBaud, menu_callback, app);
    submenu_add_item(app->menu, "Serial settings", MenuSettings, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), exit_app);
    view_dispatcher_add_view(app->dispatcher, AppViewMenu, submenu_get_view(app->menu));
    update_menu_header(app);

    app->list = submenu_alloc();
    view_dispatcher_add_view(app->dispatcher, AppViewList, submenu_get_view(app->list));

    app->settings_view = settings_view_alloc(app);
    view_dispatcher_add_view(app->dispatcher, AppViewSettings, app->settings_view);

    app->message_view = view_alloc();
    view_set_context(app->message_view, app);
    view_allocate_model(app->message_view, ViewModelTypeLocking, sizeof(MessageModel));
    view_set_draw_callback(app->message_view, message_draw);
    view_set_input_callback(app->message_view, message_input);
    view_dispatcher_add_view(app->dispatcher, AppViewMessage, app->message_view);

    app->echo_view = echo_view_alloc(app);
    view_dispatcher_add_view(app->dispatcher, AppViewEcho, app->echo_view);

    app->sniffer_view = sniffer_view_alloc(app);
    view_dispatcher_add_view(app->dispatcher, AppViewSniffer, app->sniffer_view);

    app->cmdset_view = cmdset_view_alloc(app);
    view_dispatcher_add_view(app->dispatcher, AppViewCmdSet, app->cmdset_view);

    app->visca_view = visca_view_alloc(app);
    view_dispatcher_add_view(app->dispatcher, AppViewVisca, app->visca_view);

    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    if(app->port)
        app_switch(app, AppViewMenu);
    else
        app_show_message(
            app, "Serial port busy", "Another app is using the", "serial port. Close it and", "start Cereal Tools again.",
            VIEW_NONE);
    view_dispatcher_run(app->dispatcher);

    view_dispatcher_remove_view(app->dispatcher, AppViewMenu);
    view_dispatcher_remove_view(app->dispatcher, AppViewList);
    view_dispatcher_remove_view(app->dispatcher, AppViewSettings);
    view_dispatcher_remove_view(app->dispatcher, AppViewMessage);
    view_dispatcher_remove_view(app->dispatcher, AppViewEcho);
    echo_view_free(app->echo_view);
    view_dispatcher_remove_view(app->dispatcher, AppViewSniffer);
    sniffer_view_free(app->sniffer_view);
    view_dispatcher_remove_view(app->dispatcher, AppViewCmdSet);
    cmdset_view_free(app->cmdset_view);
    view_dispatcher_remove_view(app->dispatcher, AppViewVisca);
    visca_view_free(app->visca_view);
    catalog_free();
    submenu_free(app->menu);
    submenu_free(app->list);
    settings_view_free(app->settings_view);
    view_free(app->message_view);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    serial_port_close(app->port);
    free(app);
    return 0;
}
