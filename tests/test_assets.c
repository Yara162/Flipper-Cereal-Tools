// Every built-in command file must load, render every command and expected reply, and carry
// the known values pinned below (from the manufacturers' public documents).
#include "check.h"
#include "cmdfile.h"
#include "render.h"
#include <dirent.h>
#include <stdlib.h>

#define ASSETS "../flipper-app/assets"

static CommandSet set;
static char ids[64][32];
static int id_count, file_count;

static bool load(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    static char buf[16384];
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[len] = 0;
    CmdFileError err;
    if(!cmdfile_parse(buf, len, &set, &err)) {
        printf("%s: line %d: %s\n", path, err.line, err.message);
        return false;
    }
    return true;
}

static void check_file(const char* path, const char* category) {
    file_count++;
    if(!load(path)) {
        check_failures++;
        return;
    }
    if(strcmp(category, "examples") != 0) {
        CHECK(strcmp(set.category, category) == 0);
        CHECK(!set.tested);
        CHECK(set.has_serial);
        CHECK(set.source[0] != 0);
        CHECK(strcmp(set.segment, "commercial") == 0 || strcmp(set.segment, "residential") == 0 ||
              strcmp(set.segment, "both") == 0 || set.segment[0] == 0);
    }
    for(int i = 0; i < id_count; i++)
        if(strcmp(ids[i], set.id) == 0) {
            printf("%s: duplicate id %s\n", path, set.id);
            check_failures++;
        }
    snprintf(ids[id_count++], sizeof(ids[0]), "%s", set.id);
    for(uint8_t i = 0; i < set.command_count; i++) {
        const Command* c = &set.commands[i];
        uint8_t out[256], mask[256];
        char err[64];
        if(render_bytes(c->data, c->kind, &set, out, sizeof(out), err, sizeof(err)) < 0) {
            printf("%s: command \"%s\": %s\n", path, c->name, err);
            check_failures++;
        }
        if(c->has_expect &&
           render_pattern(c->expect, c->expect_kind, &set, out, mask, sizeof(out), err, sizeof(err)) < 0) {
            printf("%s: expected reply of \"%s\": %s\n", path, c->name, err);
            check_failures++;
        }
    }
}

static void walk(const char* sub) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%s", ASSETS, sub);
    DIR* d = opendir(dir);
    if(!d) return;
    struct dirent* e;
    while((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if(n < 6 || strcmp(e->d_name + n - 5, ".json") != 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        const char* category = strcmp(sub, "displays") == 0 ? "display" : strcmp(sub, "projectors") == 0 ? "projector" : "examples";
        check_file(path, category);
    }
    closedir(d);
}

// Renders the named command of a file with default settings and compares it to `want`
static void pin(const char* file, const char* command, const uint8_t* want, size_t want_len) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", ASSETS, file);
    if(!load(path)) {
        printf("pin: cannot load %s\n", file);
        check_failures++;
        return;
    }
    for(uint8_t i = 0; i < set.command_count; i++) {
        if(strcmp(set.commands[i].name, command) != 0) continue;
        uint8_t out[256];
        char err[64];
        int n = render_bytes(set.commands[i].data, set.commands[i].kind, &set, out, sizeof(out), err, sizeof(err));
        CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want, want_len);
        return;
    }
    printf("pin: %s has no command \"%s\"\n", file, command);
    check_failures++;
}

#define PIN(file, command, ...)                                         \
    do {                                                                \
        const uint8_t _w[] = {__VA_ARGS__};                             \
        pin(file, command, _w, sizeof(_w));                             \
    } while(0)
#define PIN_TEXT(file, command, text) pin(file, command, (const uint8_t*)(text), strlen(text))

static void test_all_files_load_and_render(void) {
    walk("displays");
    walk("projectors");
    walk("examples");
    CHECK(file_count >= 6);
}

static void test_pinned_values(void) {
    PIN("displays/samsung_mdc.json", "Power on", 0xAA, 0x11, 0x01, 0x01, 0x01, 0x14);
    PIN("displays/sony_bravia.json", "Power on", 0x8C, 0x00, 0x00, 0x02, 0x01, 0x8F);
    PIN("displays/sony_bravia.json", "Power off", 0x8C, 0x00, 0x00, 0x02, 0x00, 0x8E);
    PIN("displays/sony_bravia.json", "HDMI 1", 0x8C, 0x00, 0x02, 0x03, 0x04, 0x01, 0x96);
    PIN("displays/sony_bravia.json", "Allow power on by serial", 0x8C, 0x00, 0x01, 0x02, 0x01, 0x90);
    PIN_TEXT("displays/lg_ascii.json", "Power off", "ka 01 00\r");
    PIN_TEXT("displays/lg_ascii.json", "HDMI 1", "xb 01 90\r");
    PIN_TEXT("projectors/benq.json", "Power on", "\r*pow=on#\r");
    PIN_TEXT("projectors/epson_escvp21.json", "Power on", "PWR ON\r");
    PIN_TEXT("projectors/epson_escvp21.json", "Lamp hours", "LAMP?\r");
}

int main(void) {
    RUN(test_all_files_load_and_render);
    RUN(test_pinned_values);
    DONE("test_assets");
}
