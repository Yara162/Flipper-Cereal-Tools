#include "log.h"
#include "app.h"
#include <datetime/datetime.h>

#define LOG_DIR APP_DATA "/logs"

File* log_open(const char* tool, const SerialConfig* cfg, char* name_out, size_t name_size) {
    DateTime t;
    furi_hal_rtc_get_datetime(&t);
    char name[48];
    snprintf(
        name, sizeof(name), "%s_%04u%02u%02u_%02u%02u%02u.txt", tool, t.year, t.month, t.day, t.hour, t.minute,
        t.second);
    if(name_out) snprintf(name_out, name_size, "%s", name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA);
    storage_simply_mkdir(storage, LOG_DIR);
    FuriString* path = furi_string_alloc_printf("%s/%s", LOG_DIR, name);
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    furi_string_free(path);
    if(!ok) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    char settings[24], header[128];
    serial_config_label(cfg, settings, sizeof(settings));
    snprintf(
        header, sizeof(header), "Cereal Tools: %s\nDate: %04u-%02u-%02u %02u:%02u:%02u\nSerial: %s\n\n", tool, t.year,
        t.month, t.day, t.hour, t.minute, t.second, settings);
    storage_file_write(file, header, strlen(header));
    return file;
}

void log_line(File* file, const char* text) {
    if(!file) return;
    storage_file_write(file, text, strlen(text));
    storage_file_write(file, "\n", 1);
}

void log_close(File* file) {
    if(!file) return;
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
