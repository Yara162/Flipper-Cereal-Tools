// The command screen shared by Display control, Projector control and Custom commands.
#pragma once
#include "app.h"

View* cmdset_view_alloc(App* app);
void cmdset_view_free(View* view);
// Loads a command file and shows it. Back returns to `back`.
void cmdset_view_open(App* app, const char* path, uint32_t back);
// The command screen's CommandSet, lent to the catalog while building lists (saves 19 KB).
CommandSet* cmdset_shared_set(void);
