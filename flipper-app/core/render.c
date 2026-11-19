#include "render.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct {
    uint8_t* bytes;
    uint8_t* mask; // NULL unless rendering an expected reply
    size_t max;
    size_t len;
    char* err;
    size_t err_size;
} Out;

static int error(Out* o, const char* fmt, const char* arg) {
    if(o->err && o->err_size) snprintf(o->err, o->err_size, fmt, arg ? arg : "");
    return -1;
}

static bool put(Out* o, uint8_t byte, uint8_t mask) {
    if(o->len >= o->max) return false;
    o->bytes[o->len] = byte;
    if(o->mask) o->mask[o->len] = mask;
    o->len++;
    return true;
}

static int hex_value(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static const CmdSetting* find_setting(const CommandSet* set, const char* key) {
    for(uint8_t i = 0; i < set->setting_count; i++)
        if(strcmp(set->settings[i].key, key) == 0) return &set->settings[i];
    return NULL;
}

// Reads "{name}" or "{name:fmt}" starting at p (which points at '{'). Returns the character
// after '}', or NULL if the placeholder is not closed.
static const char* read_placeholder(const char* p, char* name, size_t name_size, char* fmt, size_t fmt_size) {
    const char* close = strchr(p, '}');
    if(!close) return NULL;
    const char* colon = memchr(p, ':', (size_t)(close - p));
    const char* name_end = colon ? colon : close;
    size_t n = (size_t)(name_end - (p + 1));
    if(n >= name_size) n = name_size - 1;
    memcpy(name, p + 1, n);
    name[n] = 0;
    fmt[0] = 0;
    if(colon) {
        size_t f = (size_t)(close - (colon + 1));
        if(f >= fmt_size) f = fmt_size - 1;
        memcpy(fmt, colon + 1, f);
        fmt[f] = 0;
    }
    return close + 1;
}

static int render_hex(const char* p, const CommandSet* set, Out* o, bool wildcards) {
    while(*p) {
        if(isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if(*p == '{') {
            char name[16], fmt[8];
            const char* next = read_placeholder(p, name, sizeof(name), fmt, sizeof(fmt));
            if(!next) return error(o, "Missing } in command", NULL);
            uint32_t value;
            if(strcmp(name, "sum") == 0 || strcmp(name, "sum1") == 0) {
                value = 0;
                for(size_t i = strcmp(name, "sum1") == 0 ? 1 : 0; i < o->len; i++) value += o->bytes[i];
                value &= 0xFF;
            } else if(strcmp(name, "xor") == 0) {
                value = 0;
                for(size_t i = 0; i < o->len; i++) value ^= o->bytes[i];
            } else {
                const CmdSetting* s = find_setting(set, name);
                if(!s) return error(o, "Unknown setting {%s}", name);
                if(s->value < 0 || s->value > 255) return error(o, "{%s} does not fit in one byte", name);
                value = (uint32_t)s->value;
            }
            if(!put(o, (uint8_t)value, 0xFF)) return error(o, "Command too long", NULL);
            p = next;
            continue;
        }
        if(p[0] == '?' && p[1] == '?') {
            if(!wildcards) return error(o, "?? is only allowed in expected replies", NULL);
            if(!put(o, 0, 0)) return error(o, "Reply pattern too long", NULL);
            p += 2;
            continue;
        }
        int hi = hex_value(p[0]);
        int lo = p[1] ? hex_value(p[1]) : -1;
        if(hi < 0 || lo < 0) return error(o, "Bad hex near \"%.4s\"", p);
        if(!put(o, (uint8_t)(hi * 16 + lo), 0xFF)) return error(o, "Command too long", NULL);
        p += 2;
    }
    return (int)o->len;
}

static int render_text(const char* p, const CommandSet* set, Out* o) {
    while(*p) {
        if(*p == '\\' && p[1]) {
            char e = p[1];
            uint8_t byte;
            if(e == 'r')
                byte = '\r';
            else if(e == 'n')
                byte = '\n';
            else if(e == 't')
                byte = '\t';
            else if(e == 'x') {
                int hi = p[2] ? hex_value(p[2]) : -1;
                int lo = (hi >= 0 && p[3]) ? hex_value(p[3]) : -1;
                if(hi < 0 || lo < 0) return error(o, "Bad \\x escape", NULL);
                byte = (uint8_t)(hi * 16 + lo);
                p += 2;
            } else
                byte = (uint8_t)e; // \\ and anything else: the character itself
            if(!put(o, byte, 0xFF)) return error(o, "Command too long", NULL);
            p += 2;
            continue;
        }
        if(p[0] == '{' && p[1] == '{') { // literal brace
            if(!put(o, '{', 0xFF)) return error(o, "Command too long", NULL);
            p += 2;
            continue;
        }
        if(*p == '{') {
            char name[16], fmt[8];
            const char* next = read_placeholder(p, name, sizeof(name), fmt, sizeof(fmt));
            if(!next) return error(o, "Missing } in command", NULL);
            if(strcmp(name, "sum") == 0 || strcmp(name, "sum1") == 0 || strcmp(name, "xor") == 0)
                return error(o, "{%s} only works in hex commands", name);
            const CmdSetting* s = find_setting(set, name);
            if(!s) return error(o, "Unknown setting {%s}", name);
            char number[16];
            if(!fmt[0]) {
                snprintf(number, sizeof(number), "%ld", (long)s->value);
            } else {
                int width = fmt[0] - '0';
                char type = fmt[1];
                if(width < 1 || width > 4 || fmt[2] || (type != 'd' && type != 'x' && type != 'X'))
                    return error(o, "Bad format in {%s}", name);
                const char* f = type == 'd' ? "%0*ld" : type == 'x' ? "%0*lx" : "%0*lX";
                snprintf(number, sizeof(number), f, width, (long)s->value);
            }
            for(const char* q = number; *q; q++)
                if(!put(o, (uint8_t)*q, 0xFF)) return error(o, "Command too long", NULL);
            p = next;
            continue;
        }
        if(!put(o, (uint8_t)*p, 0xFF)) return error(o, "Command too long", NULL);
        p++;
    }
    return (int)o->len;
}

int render_bytes(
    const char* data,
    DataKind kind,
    const CommandSet* set,
    uint8_t* out,
    size_t out_max,
    char* err,
    size_t err_size) {
    Out o = {.bytes = out, .mask = NULL, .max = out_max, .err = err, .err_size = err_size};
    return kind == DataHex ? render_hex(data, set, &o, false) : render_text(data, set, &o);
}

int render_pattern(
    const char* data,
    DataKind kind,
    const CommandSet* set,
    uint8_t* bytes,
    uint8_t* mask,
    size_t max,
    char* err,
    size_t err_size) {
    Out o = {.bytes = bytes, .mask = mask, .max = max, .err = err, .err_size = err_size};
    return kind == DataHex ? render_hex(data, set, &o, true) : render_text(data, set, &o);
}

bool render_match(
    const uint8_t* reply,
    size_t reply_len,
    const uint8_t* bytes,
    const uint8_t* mask,
    size_t pattern_len) {
    if(pattern_len == 0) return true;
    if(reply_len < pattern_len) return false;
    for(size_t start = 0; start + pattern_len <= reply_len; start++) {
        size_t i = 0;
        while(i < pattern_len && (reply[start + i] & mask[i]) == (bytes[i] & mask[i])) i++;
        if(i == pattern_len) return true;
    }
    return false;
}

// Finds `needle` (as bytes) in the reply; returns its offset or -1
static long find(const uint8_t* hay, size_t len, const char* needle, size_t from) {
    size_t n = strlen(needle);
    if(n == 0) return (long)from;
    for(size_t i = from; i + n <= len; i++)
        if(memcmp(hay + i, needle, n) == 0) return (long)i;
    return -1;
}

static void apply_map(const ShowRule* rule, const char* raw, const char* hex_form, char* out, size_t out_size) {
    for(uint8_t i = 0; i < rule->map_count; i++) {
        if(strcasecmp(rule->map[i].raw, raw) == 0 || (hex_form && strcasecmp(rule->map[i].raw, hex_form) == 0)) {
            snprintf(out, out_size, "%s", rule->map[i].word);
            return;
        }
    }
    snprintf(out, out_size, "%s", raw);
}

bool render_show(const ShowRule* rule, const uint8_t* reply, size_t len, char* out, size_t out_size) {
    if(out_size) out[0] = 0;
    if(rule->binary) {
        if(rule->byte < 0 || (size_t)rule->byte + rule->length > len) return false;
        uint32_t value = 0;
        for(uint8_t i = 0; i < rule->length; i++) {
            uint8_t b = reply[rule->byte + (rule->little ? rule->length - 1 - i : i)];
            value = (value << 8) | b;
        }
        char decimal[12], hex_form[12];
        snprintf(decimal, sizeof(decimal), "%lu", (unsigned long)value);
        int width = (rule->length > 4 ? 4 : rule->length) * 2;
        snprintf(hex_form, sizeof(hex_form), "%0*lX", width & 15, (unsigned long)value & 0xFFFFFFFFUL);
        apply_map(rule, decimal, hex_form, out, out_size);
        return true;
    }
    long start = find(reply, len, rule->after, 0);
    if(start < 0) return false;
    size_t from = (size_t)start + strlen(rule->after);
    long end = rule->until[0] ? find(reply, len, rule->until, from) : -1;
    size_t stop = end >= 0 ? (size_t)end : len;
    char raw[40];
    size_t n = stop - from;
    if(n >= sizeof(raw)) n = sizeof(raw) - 1;
    for(size_t i = 0; i < n; i++) {
        uint8_t c = reply[from + i];
        raw[i] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    raw[n] = 0;
    apply_map(rule, raw, NULL, out, out_size);
    return true;
}
