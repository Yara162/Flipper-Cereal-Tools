// Custom commands: the owner's own JSON files in apps_data/cereal_tools/commands.
#include "tool_custom.h"
#include "catalog.h"

void custom_open(App* app) {
    bool seeded = catalog_seed_example();
    catalog_open_picker(app, seeded ? "Example added" : "Custom commands", NULL, false, NULL, AppViewMenu);
}
