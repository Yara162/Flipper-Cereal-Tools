// Finding command files on the SD card, and the shared picker list that shows them.
#pragma once
#include "app.h"

#define CATALOG_MAX 32
#define COMMANDS_DIR APP_DATA "/commands"

// category "display", "projector" or NULL (the owner's files of any category). With
// include_builtin, the app's own files for that category are listed too. `segment` filters
// displays ("commercial", "residential"; files marked "both" match either); NULL lists all.
size_t catalog_scan(const char* category, bool include_builtin, const char* segment, CatalogEntry* out, size_t max);

// Fills the shared list with the entries and shows it. Picking one opens it in the command
// screen; Back from the list returns to `back`.
void catalog_open_picker(App* app, const char* header, const char* category, bool include_builtin, const char* segment, uint32_t back);

// Copies the bundled example into the owner's commands folder if that folder has no .json files.
// Returns true if it copied the example.
bool catalog_seed_example(void);
void catalog_free(void);
