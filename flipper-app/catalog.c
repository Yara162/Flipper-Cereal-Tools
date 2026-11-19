#include "catalog.h"
#include "cmdset_view.h"
#include <storage/storage.h>
#include "core/fmt.h"

#define FILE_MAX 8192

typedef struct {
    CatalogEntry entries[CATALOG_MAX];
    size_t count;
} Picker;

static uint32_t picker_back = AppViewMenu;
static Picker* picker;

static bool ends_with_json(const char* name) {
    size_t n = strlen(name);
    return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}

static bool segment_matches(const CommandSet* set, const char* segment) {
    if(!segment) return true;
    return strcmp(set->segment, segment) == 0 || strcmp(set->segment, "both") == 0;
}

// Reads every .json in `dir` into `out`, filtered by category and segment
static size_t scan_dir(
    Storage* storage,
    const char* dir,
    bool user,
    const char* category,
    const char* segment,
    CommandSet* set,
    char* buf,
    CatalogEntry* out,
    size_t max) {
    size_t n = 0;
    File* d = storage_file_alloc(storage);
    if(storage_dir_open(d, dir)) {
        FileInfo info;
        char name[64];
        while(n < max && storage_dir_read(d, &info, name, sizeof(name))) {
            if((info.flags & FSF_DIRECTORY) || !ends_with_json(name)) continue;
            CatalogEntry* e = &out[n];
            memset(e, 0, sizeof(*e));
            snprintf(e->path, sizeof(e->path), "%s/%s", dir, name);
            e->user = user;
            File* f = storage_file_alloc(storage);
            size_t len = 0;
            if(storage_file_open(f, e->path, FSAM_READ, FSOM_OPEN_EXISTING)) len = storage_file_read(f, buf, FILE_MAX - 1);
            storage_file_close(f);
            storage_file_free(f);
            buf[len] = 0;
            CmdFileError err;
            if(len == FILE_MAX - 1) {
                e->broken = true;
                snprintf(e->error, sizeof(e->error), "File is larger than 8 KB");
            } else if(!cmdfile_parse(buf, len, set, &err)) {
                e->broken = true;
                e->error_line = err.line;
                snprintf(e->error, sizeof(e->error), "%s", err.message);
            }
            if(e->broken) {
                if(!user) continue; // a broken built-in file is a build problem, not the owner's
                snprintf(e->name, sizeof(e->name), "%.31s", name);
                n++;
                continue;
            }
            if(category && strcmp(set->category, category) != 0) continue;
            if(!segment_matches(set, segment)) continue;
            snprintf(e->id, sizeof(e->id), "%s", set->id);
            snprintf(e->name, sizeof(e->name), "%s", set->name);
            snprintf(e->segment, sizeof(e->segment), "%s", set->segment);
            snprintf(e->category, sizeof(e->category), "%s", set->category);
            e->tested = set->tested;
            n++;
        }
    }
    storage_dir_close(d);
    storage_file_free(d);
    return n;
}

size_t catalog_scan(const char* category, bool include_builtin, const char* segment, CatalogEntry* out, size_t max) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    CommandSet* set = cmdset_shared_set(); // the command screen is not showing while lists are built
    char* buf = malloc(FILE_MAX);
    CatalogEntry* builtin = malloc(sizeof(CatalogEntry) * CATALOG_MAX);
    CatalogEntry* user = malloc(sizeof(CatalogEntry) * CATALOG_MAX);
    size_t nb = 0, nu;
    if(include_builtin && category) {
        char dir[48];
        snprintf(dir, sizeof(dir), "%s/%ss", APP_ASSETS, category); // displays, projectors
        nb = scan_dir(storage, dir, false, category, segment, set, buf, builtin, CATALOG_MAX);
    }
    nu = scan_dir(storage, COMMANDS_DIR, true, category, segment, set, buf, user, CATALOG_MAX);
    size_t n = catalog_merge(builtin, nb, user, nu, out, max);
    free(builtin);
    free(user);
    free(buf);
    furi_record_close(RECORD_STORAGE);
    return n;
}

bool catalog_seed_example(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA);
    storage_simply_mkdir(storage, COMMANDS_DIR);
    bool any = false;
    File* d = storage_file_alloc(storage);
    if(storage_dir_open(d, COMMANDS_DIR)) {
        FileInfo info;
        char name[64];
        while(!any && storage_dir_read(d, &info, name, sizeof(name)))
            if(!(info.flags & FSF_DIRECTORY) && ends_with_json(name)) any = true;
    }
    storage_dir_close(d);
    storage_file_free(d);
    bool copied = false;
    if(!any)
        copied = storage_common_copy(storage, APP_ASSETS "/examples/my_device.json", COMMANDS_DIR "/my_device.json") ==
                 FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return copied;
}

static void pick(void* context, uint32_t index) {
    App* app = context;
    if(!picker || index >= picker->count) return;
    const CatalogEntry* e = &picker->entries[index];
    if(e->broken) {
        char title[24], name[26], l1[26], l2[26];
        snprintf(title, sizeof(title), "Error, line %d", e->error_line);
        snprintf(name, sizeof(name), "%.25s", e->name);
        fmt_wrap(e->error, 25, l1, l2);
        app_show_message(app, title, name, l1, l2, AppViewList);
        return;
    }
    cmdset_view_open(app, e->path, AppViewList);
}

static uint32_t list_back(void* context) {
    UNUSED(context);
    return picker_back;
}

void catalog_open_picker(App* app, const char* header, const char* category, bool include_builtin, const char* segment, uint32_t back) {
    if(!picker) picker = malloc(sizeof(Picker));
    picker->count = catalog_scan(category, include_builtin, segment, picker->entries, CATALOG_MAX);
    picker_back = back;
    submenu_reset(app->list);
    submenu_set_header(app->list, header);
    for(size_t i = 0; i < picker->count; i++) {
        const CatalogEntry* e = &picker->entries[i];
        char label[48];
        if(e->broken)
            snprintf(label, sizeof(label), "! %s", e->name);
        else
            snprintf(label, sizeof(label), "%s%s%s", e->name, e->tested || e->user ? "" : " *", e->user ? " (mine)" : "");
        submenu_add_item(app->list, label, (uint32_t)i, pick, app);
    }
    view_set_previous_callback(submenu_get_view(app->list), list_back);
    if(picker->count == 0) {
        app_show_message(app, header, "No command files found.", "Add .json files to", "apps_data/cereal_tools/commands", back);
        return;
    }
    app_switch(app, AppViewList);
}

void catalog_free(void) {
    free(picker);
    picker = NULL;
}
