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
