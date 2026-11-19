// Shared serial settings: stored on the SD card, edited on one screen.
#pragma once
#include "app.h"

void settings_load(SerialConfig* cfg);
void settings_save(const SerialConfig* cfg);
View* settings_view_alloc(App* app);
void settings_view_free(View* view);
