// Display control and Projector control: pickers over the built-in and owner command sets.
#include "tool_devices.h"
#include "catalog.h"

typedef enum { SegmentCommercial, SegmentResidential } Segment;

static void segment_picked(void* context, uint32_t index) {
    App* app = context;
    bool commercial = index == SegmentCommercial;
    catalog_open_picker(
        app, commercial ? "Commercial displays" : "Residential TVs", "display", true,
        commercial ? "commercial" : "residential", AppViewMenu);
}

static uint32_t back_to_menu(void* context) {
    UNUSED(context);
    return AppViewMenu;
}

void display_open(App* app) {
    submenu_reset(app->list);
    submenu_set_header(app->list, "Display control");
    submenu_add_item(app->list, "Commercial displays", SegmentCommercial, segment_picked, app);
    submenu_add_item(app->list, "Residential TVs", SegmentResidential, segment_picked, app);
    view_set_previous_callback(submenu_get_view(app->list), back_to_menu);
    app_switch(app, AppViewList);
}

void projector_open(App* app) {
    catalog_open_picker(app, "Projector control", "projector", true, NULL, AppViewMenu);
}
