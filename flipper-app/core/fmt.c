#include "fmt.h"
#include <stdio.h>
#include <string.h>

size_t fmt_text(const uint8_t* data, size_t len, char* out, size_t out_size) {
    if(out_size == 0) return 0;
    size_t n = 0;
    for(size_t i = 0; i < len; i++) {
        char piece[5];
        uint8_t c = data[i];
        if(c == '\r')
            strcpy(piece, "\\r");
        else if(c == '\n')
            strcpy(piece, "\\n");
        else if(c == '\t')
            strcpy(piece, "\\t");
        else if(c == '\\')
            strcpy(piece, "\\\\");
        else if(c >= 32 && c < 127) {
            piece[0] = (char)c;
            piece[1] = 0;
        } else
            snprintf(piece, sizeof(piece), "\\x%02X", c);
        size_t p = strlen(piece);
        if(n + p + 1 > out_size) break;
        memcpy(out + n, piece, p);
        n += p;
    }
    out[n] = 0;
    return n;
}

size_t fmt_hex(const uint8_t* data, size_t len, char* out, size_t out_size) {
    if(out_size == 0) return 0;
    size_t n = 0;
    for(size_t i = 0; i < len; i++) {
        size_t need = (i ? 3 : 2);
        if(n + need + 1 > out_size) break;
        snprintf(out + n, out_size - n, i ? " %02X" : "%02X", data[i]);
        n += need;
    }
    out[n] = 0;
    return n;
}

size_t fmt_message(const Message* m, bool hex, char* out, size_t out_size) {
    if(out_size < 3) return 0;
    // keep room for the continuation mark
    size_t room = out_size - (m->truncated ? 2 : 0);
    size_t n = hex ? fmt_hex(m->data, m->len, out, room) : fmt_text(m->data, m->len, out, room);
    if(m->truncated) {
        if(hex && n) out[n++] = ' ';
        out[n++] = '+';
        out[n] = 0;
    }
    return n;
}

void fmt_wrap(const char* text, size_t width, char* line1, char* line2) {
    size_t len = strlen(text);
    if(len <= width) {
        memcpy(line1, text, len + 1);
        line2[0] = 0;
        return;
    }
    size_t cut = width;
    while(cut > 0 && text[cut] != ' ') cut--;
    size_t next = cut;
    if(cut == 0) {
        cut = next = width; // one long word: hard break
    } else {
        next = cut + 1; // drop the space at the break
    }
    memcpy(line1, text, cut);
    line1[cut] = 0;
    size_t rest = strlen(text + next);
    if(rest > width) rest = width;
    memcpy(line2, text + next, rest);
    line2[rest] = 0;
}
