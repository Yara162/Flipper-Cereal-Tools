// Log files: SD Card/apps_data/cereal_tools/logs/<tool>_<date>_<time>.txt
#pragma once
#include <storage/storage.h>
#include "core/cmdfile.h"

// Opens a new log and writes its header (tool, date, serial settings). NULL on failure.
File* log_open(const char* tool, const SerialConfig* cfg, char* name_out, size_t name_size);
void log_line(File* file, const char* text);
void log_close(File* file);
