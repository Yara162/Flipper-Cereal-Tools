#include "cmdfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#define JSMN_STRICT
#define JSMN_PARENT_LINKS
#include "../lib/jsmn/jsmn.h"

typedef struct {
    const char* js;
    size_t len;
    jsmntok_t* tok;
    int count;
    CmdFileError* err;
} Ctx;

static int line_at(const Ctx* c, int pos) {
    int line = 1;
    for(int i = 0; i < pos && (size_t)i < c->len; i++)
        if(c->js[i] == '\n') line++;
    return line;
}

static bool fail(Ctx* c, int pos, const char* fmt, const char* arg) {
    c->err->line = line_at(c, pos);
    snprintf(c->err->message, sizeof(c->err->message), fmt, arg ? arg : "");
    return false;
}

static bool fail_tok(Ctx* c, int t, const char* fmt, const char* arg) {
    return fail(c, c->tok[t].start, fmt, arg);
}

// Index just past token t and everything inside it
static int skip(const Ctx* c, int t) {
    int n = c->tok[t].size;
    int j = t + 1;
    for(int i = 0; i < n; i++) j = skip(c, j);
    return j;
}

// Every key in every object must be a string holding exactly one value. jsmn does not check
// this itself, and it is how a missing comma or colon shows up.
static bool validate(Ctx* c) {
    for(int t = 0; t < c->count; t++) {
        if(c->tok[t].type != JSMN_OBJECT) continue;
        int j = t + 1;
        for(int k = 0; k < c->tok[t].size; k++) {
            if(c->tok[j].type != JSMN_STRING) return fail_tok(c, j, "Expected a name in quotes", NULL);
            if(c->tok[j].size == 0) return fail_tok(c, j, "Missing colon or value", NULL);
            if(c->tok[j].size > 1) {
                int second = skip(c, j + 1);
                return fail_tok(c, second, "Missing comma", NULL);
            }
            j = skip(c, j);
        }
    }
    return true;
}

static bool tok_is(const Ctx* c, int t, const char* s) {
    int n = c->tok[t].end - c->tok[t].start;
    return (int)strlen(s) == n && strncmp(c->js + c->tok[t].start, s, (size_t)n) == 0;
}

// Copies a string token, decoding JSON escapes. Returns false if it does not fit.
static bool get_string(Ctx* c, int t, char* out, size_t size, bool must_fit) {
    if(c->tok[t].type != JSMN_STRING) return fail_tok(c, t, "Expected text in quotes", NULL);
    size_t n = 0;
    bool fits = true;
    for(int i = c->tok[t].start; i < c->tok[t].end; i++) {
        char ch = c->js[i];
        if(ch == '\\' && i + 1 < c->tok[t].end) {
            char e = c->js[++i];
            switch(e) {
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'u': {
                char hex[5] = {0};
                for(int k = 0; k < 4 && i + 1 < c->tok[t].end; k++) hex[k] = c->js[++i];
                long v = strtol(hex, NULL, 16);
                ch = (v > 0 && v < 128) ? (char)v : '?';
                break;
            }
            default: ch = e; break; // \" \\ \/
            }
        }
        if(n + 1 < size)
            out[n++] = ch;
        else
            fits = false;
    }
    out[n] = 0;
    if(!fits && must_fit) return fail_tok(c, t, "Text too long", NULL);
    return true;
}

static bool get_int(Ctx* c, int t, int32_t* out) {
    char buf[16];
    int n = c->tok[t].end - c->tok[t].start;
    if(c->tok[t].type != JSMN_PRIMITIVE || n <= 0 || n >= (int)sizeof(buf))
        return fail_tok(c, t, "Expected a number", NULL);
    memcpy(buf, c->js + c->tok[t].start, (size_t)n);
    buf[n] = 0;
    char* end;
    long v = strtol(buf, &end, 10);
    if(*end) return fail_tok(c, t, "Expected a whole number", NULL);
    *out = (int32_t)v;
    return true;
}

static bool get_bool(Ctx* c, int t, bool* out) {
    if(tok_is(c, t, "true"))
        *out = true;
    else if(tok_is(c, t, "false"))
        *out = false;
    else
        return fail_tok(c, t, "Expected true or false", NULL);
    return true;
}

static bool parse_serial(Ctx* c, int t, SerialConfig* s) {
    if(c->tok[t].type != JSMN_OBJECT) return fail_tok(c, t, "serial must be { ... }", NULL);
    s->baud = 9600;
    s->data_bits = 8;
    s->parity = 'N';
    s->stop_bits = 1;
    int j = t + 1;
    for(int k = 0; k < c->tok[t].size; k++, j = skip(c, j)) {
        int v = j + 1;
        int32_t n;
        if(tok_is(c, j, "baud")) {
            if(!get_int(c, v, &n)) return false;
            if(n < 300 || n > 1000000) return fail_tok(c, v, "Baud rate out of range", NULL);
            s->baud = (uint32_t)n;
        } else if(tok_is(c, j, "data_bits")) {
            if(!get_int(c, v, &n)) return false;
            if(n != 7 && n != 8) return fail_tok(c, v, "data_bits must be 7 or 8", NULL);
            s->data_bits = (uint8_t)n;
        } else if(tok_is(c, j, "stop_bits")) {
            if(!get_int(c, v, &n)) return false;
            if(n != 1 && n != 2) return fail_tok(c, v, "stop_bits must be 1 or 2", NULL);
            s->stop_bits = (uint8_t)n;
        } else if(tok_is(c, j, "parity")) {
            if(tok_is(c, v, "none"))
                s->parity = 'N';
            else if(tok_is(c, v, "even"))
                s->parity = 'E';
            else if(tok_is(c, v, "odd"))
                s->parity = 'O';
            else
                return fail_tok(c, v, "parity must be none, even or odd", NULL);
        } else {
            char key[24];
            get_string(c, j, key, sizeof(key), false);
            return fail_tok(c, j, "Unknown serial field: %s", key);
        }
    }
    return true;
}

static bool parse_settings(Ctx* c, int t, CommandSet* set) {
    if(c->tok[t].type != JSMN_ARRAY) return fail_tok(c, t, "settings must be [ ... ]", NULL);
    int e = t + 1;
    for(int i = 0; i < c->tok[t].size; i++, e = skip(c, e)) {
        if(set->setting_count >= CMDSET_MAX_SETTINGS) return fail_tok(c, e, "More than 4 settings", NULL);
        if(c->tok[e].type != JSMN_OBJECT) return fail_tok(c, e, "Each setting must be { ... }", NULL);
        CmdSetting* s = &set->settings[set->setting_count];
        memset(s, 0, sizeof(*s));
        s->max = 255;
        int j = e + 1;
        for(int k = 0; k < c->tok[e].size; k++, j = skip(c, j)) {
            int v = j + 1;
            bool ok;
            if(tok_is(c, j, "key"))
                ok = get_string(c, v, s->key, sizeof(s->key), true);
            else if(tok_is(c, j, "label"))
                ok = get_string(c, v, s->label, sizeof(s->label), false);
            else if(tok_is(c, j, "min"))
                ok = get_int(c, v, &s->min);
            else if(tok_is(c, j, "max"))
                ok = get_int(c, v, &s->max);
            else if(tok_is(c, j, "default"))
                ok = get_int(c, v, &s->def);
            else {
                char key[24];
                get_string(c, j, key, sizeof(key), false);
                return fail_tok(c, j, "Unknown setting field: %s", key);
            }
            if(!ok) return false;
        }
        if(!s->key[0]) return fail_tok(c, e, "Setting has no key", NULL);
        if(s->min > s->max || s->def < s->min || s->def > s->max)
            return fail_tok(c, e, "Setting default outside min..max", NULL);
        if(!s->label[0]) strncpy(s->label, s->key, sizeof(s->label) - 1);
        s->value = s->def;
        set->setting_count++;
    }
    return true;
}

static bool parse_show(Ctx* c, int t, CommandSet* set, uint8_t command) {
    if(c->tok[t].type != JSMN_ARRAY) return fail_tok(c, t, "show must be [ ... ]", NULL);
    int e = t + 1;
    for(int i = 0; i < c->tok[t].size; i++, e = skip(c, e)) {
        if(set->show_count >= CMDSET_MAX_SHOWS) return fail_tok(c, e, "More than 16 show rules", NULL);
        if(c->tok[e].type != JSMN_OBJECT) return fail_tok(c, e, "Each show rule must be { ... }", NULL);
        ShowRule* r = &set->shows[set->show_count];
        memset(r, 0, sizeof(*r));
        r->command = command;
        r->byte = -1;
        bool has_after = false;
        int j = e + 1;
        for(int k = 0; k < c->tok[e].size; k++, j = skip(c, j)) {
            int v = j + 1;
            int32_t n;
            bool ok = true;
            if(tok_is(c, j, "label"))
                ok = get_string(c, v, r->label, sizeof(r->label), false);
            else if(tok_is(c, j, "after")) {
                ok = get_string(c, v, r->after, sizeof(r->after), true);
                has_after = true;
            } else if(tok_is(c, j, "until"))
                ok = get_string(c, v, r->until, sizeof(r->until), true);
            else if(tok_is(c, j, "byte")) {
                ok = get_int(c, v, &n);
                if(ok && (n < 0 || n > 255)) return fail_tok(c, v, "byte must be 0 to 255", NULL);
                r->byte = (int16_t)n;
                r->binary = true;
            } else if(tok_is(c, j, "length")) {
                ok = get_int(c, v, &n);
                if(ok && (n < 1 || n > 4)) return fail_tok(c, v, "length must be 1 to 4", NULL);
                r->length = (uint8_t)n;
            } else if(tok_is(c, j, "order")) {
                if(tok_is(c, v, "little"))
                    r->little = true;
                else if(!tok_is(c, v, "big"))
                    return fail_tok(c, v, "order must be big or little", NULL);
            } else if(tok_is(c, j, "map")) {
                if(c->tok[v].type != JSMN_OBJECT) return fail_tok(c, v, "map must be { ... }", NULL);
                int m = v + 1;
                for(int q = 0; q < c->tok[v].size; q++, m = skip(c, m)) {
                    if(r->map_count >= 6) return fail_tok(c, m, "More than 6 map entries", NULL);
                    ShowMap* sm = &r->map[r->map_count++];
                    if(!get_string(c, m, sm->raw, sizeof(sm->raw), true)) return false;
                    if(!get_string(c, m + 1, sm->word, sizeof(sm->word), false)) return false;
                }
            } else {
                char key[24];
                get_string(c, j, key, sizeof(key), false);
                return fail_tok(c, j, "Unknown show field: %s", key);
            }
            if(!ok) return false;
        }
        if(r->binary == has_after) return fail_tok(c, e, "Show rule needs after or byte", NULL);
        if(r->binary && r->length == 0) r->length = 1;
        set->show_count++;
    }
    return true;
}

static bool parse_commands(Ctx* c, int t, CommandSet* set) {
    if(c->tok[t].type != JSMN_ARRAY) return fail_tok(c, t, "commands must be [ ... ]", NULL);
    int e = t + 1;
    for(int i = 0; i < c->tok[t].size; i++, e = skip(c, e)) {
        if(set->command_count >= CMDSET_MAX_COMMANDS) return fail_tok(c, e, "More than 48 commands", NULL);
        if(c->tok[e].type != JSMN_OBJECT) return fail_tok(c, e, "Each command must be { ... }", NULL);
        uint8_t index = set->command_count;
        Command* cmd = &set->commands[index];
        memset(cmd, 0, sizeof(*cmd));
        bool has_hex = false, has_text = false, has_name = false;
        int show_tok = -1;
        int j = e + 1;
        for(int k = 0; k < c->tok[e].size; k++, j = skip(c, j)) {
            int v = j + 1;
            bool ok;
            if(tok_is(c, j, "name")) {
                ok = get_string(c, v, cmd->name, sizeof(cmd->name), false);
                has_name = true;
            } else if(tok_is(c, j, "group"))
                ok = get_string(c, v, cmd->group, sizeof(cmd->group), false);
            else if(tok_is(c, j, "hex") || tok_is(c, j, "text")) {
                bool hex = tok_is(c, j, "hex");
                if(has_hex || has_text) return fail_tok(c, j, "Use hex or text, not both", NULL);
                ok = get_string(c, v, cmd->data, sizeof(cmd->data), true);
                cmd->kind = hex ? DataHex : DataText;
                has_hex = hex;
                has_text = !hex;
            } else if(tok_is(c, j, "expect") || tok_is(c, j, "expect_text")) {
                if(cmd->has_expect) return fail_tok(c, j, "Use expect or expect_text, not both", NULL);
                ok = get_string(c, v, cmd->expect, sizeof(cmd->expect), true);
                cmd->has_expect = true;
                cmd->expect_kind = tok_is(c, j, "expect") ? DataHex : DataText;
            } else if(tok_is(c, j, "show")) {
                show_tok = v; // parsed after the command is complete
                ok = true;
            } else {
                char key[24];
                get_string(c, j, key, sizeof(key), false);
                return fail_tok(c, j, "Unknown command field: %s", key);
            }
            if(!ok) return false;
        }
        if(!has_name) return fail_tok(c, e, "Command has no name", NULL);
        if(!has_hex && !has_text) return fail_tok(c, e, "Command needs hex or text", NULL);
        set->command_count++;
        if(show_tok >= 0 && !parse_show(c, show_tok, set, index)) return false;
    }
    return true;
}

bool cmdfile_parse(const char* json, size_t len, CommandSet* out, CmdFileError* err) {
    memset(out, 0, sizeof(*out));
    memset(err, 0, sizeof(*err));
    Ctx c = {.js = json, .len = len, .err = err};

    jsmn_parser p;
    jsmn_init(&p);
    int n = jsmn_parse(&p, json, len, NULL, 0);
    if(n < 0) {
        int pos = (int)p.pos;
        return fail(&c, pos, n == JSMN_ERROR_PART ? "File ends too early" : "Not valid JSON here", NULL);
    }
    if(n == 0) return fail(&c, 0, "File is empty", NULL);

    c.tok = malloc(sizeof(jsmntok_t) * (size_t)n);
    if(!c.tok) return fail(&c, 0, "Out of memory", NULL);
    jsmn_init(&p);
    c.count = jsmn_parse(&p, json, len, c.tok, (unsigned)n);

    bool ok = false;
    bool has_format = false, has_id = false, has_name = false;
    do {
        if(c.count < 1 || c.tok[0].type != JSMN_OBJECT) {
            fail(&c, 0, "File must be one { ... } object", NULL);
            break;
        }
        if(!validate(&c)) break;
        int j = 1;
        bool good = true;
        for(int k = 0; k < c.tok[0].size && good; k++, j = skip(&c, j)) {
            int v = j + 1;
            if(tok_is(&c, j, "format")) {
                int32_t f;
                good = get_int(&c, v, &f);
                if(good && f != 1) good = fail_tok(&c, v, "Unsupported format (need 1)", NULL);
                has_format = true;
            } else if(tok_is(&c, j, "id")) {
                good = get_string(&c, v, out->id, sizeof(out->id), true);
                has_id = true;
            } else if(tok_is(&c, j, "name")) {
                good = get_string(&c, v, out->name, sizeof(out->name), false);
                has_name = true;
            } else if(tok_is(&c, j, "category"))
                good = get_string(&c, v, out->category, sizeof(out->category), false);
            else if(tok_is(&c, j, "segment"))
                good = get_string(&c, v, out->segment, sizeof(out->segment), false);
            else if(tok_is(&c, j, "source"))
                good = get_string(&c, v, out->source, sizeof(out->source), false);
            else if(tok_is(&c, j, "tested"))
                good = get_bool(&c, v, &out->tested);
            else if(tok_is(&c, j, "serial")) {
                good = parse_serial(&c, v, &out->serial);
                out->has_serial = good;
            } else if(tok_is(&c, j, "settings"))
                good = parse_settings(&c, v, out);
            else if(tok_is(&c, j, "commands"))
                good = parse_commands(&c, v, out);
            // anything else is ignored, so later versions can add fields
        }
        if(!good) break;
        if(!has_format) {
            fail(&c, 0, "Missing \"format\": 1", NULL);
            break;
        }
        if(!has_id || !out->id[0]) {
            fail(&c, 0, "Missing \"id\"", NULL);
            break;
        }
        if(!has_name) strncpy(out->name, out->id, sizeof(out->name) - 1);
        ok = true;
    } while(0);

    free(c.tok);
    return ok;
}
