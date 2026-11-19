#include "msgring.h"
#include "core/fmt.h"
#include <stdio.h>
#include <string.h>

void msgring_clear(MsgRing* r) {
    r->count = r->head = r->scroll = 0;
}

void msgring_add(MsgRing* r, const Message* m) {
    uint8_t slot = (uint8_t)((r->head + r->count) % MSGRING_SIZE);
    if(r->count == MSGRING_SIZE)
        r->head = (uint8_t)((r->head + 1) % MSGRING_SIZE);
    else
        r->count++;
    r->items[slot] = *m;
    if(r->scroll) r->scroll = (uint8_t)(r->scroll + 1 < r->count ? r->scroll + 1 : r->scroll); // keep the view still
}

const Message* msgring_get(const MsgRing* r, uint8_t i) {
    return &r->items[(r->head + i) % MSGRING_SIZE];
}

const Message* msgring_newest(const MsgRing* r) {
    return r->count ? msgring_get(r, (uint8_t)(r->count - 1)) : NULL;
}

void msgring_scroll(MsgRing* r, int dir, uint8_t rows) {
    uint8_t max = r->count > rows ? (uint8_t)(r->count - rows) : 0;
    if(dir > 0 && r->scroll < max) r->scroll++;
    if(dir < 0 && r->scroll > 0) r->scroll--;
}

void msgring_draw(
    Canvas* canvas,
    const MsgRing* r,
    int32_t y,
    uint8_t rows,
    bool hex,
    bool show_time,
    uint32_t start_ms) {
    if(!r->count) return;
    int last = (int)r->count - 1 - r->scroll;
    int first = last - rows + 1;
    if(first < 0) first = 0;
    char line[64];
    for(int i = first; i <= last; i++) {
        const Message* m = msgring_get(r, (uint8_t)i);
        size_t n = 0;
        if(show_time) {
            uint32_t t = m->at_ms - start_ms;
            n = (size_t)snprintf(
                line, sizeof(line), "%02lu:%02lu.%03lu ", (unsigned long)(t / 60000 % 100),
                (unsigned long)(t / 1000 % 60), (unsigned long)(t % 1000));
        }
        if(hex)
            fmt_hex(m->data, m->len, line + n, sizeof(line) - n);
        else
            fmt_text(m->data, m->len, line + n, sizeof(line) - n);
        canvas_draw_str(canvas, 1, y + 9 * (i - first), line);
    }
}
