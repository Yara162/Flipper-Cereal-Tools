// A ring of recent messages and the code to draw them as a scrolling list. Used by Echo and
// the Sniffer.
#pragma once
#include <gui/canvas.h>
#include "core/msgsplit.h"

#define MSGRING_SIZE 32

typedef struct {
    Message items[MSGRING_SIZE];
    uint8_t count, head;
    uint8_t scroll; // rows up from the newest message; 0 follows new messages
} MsgRing;

void msgring_clear(MsgRing* r);
void msgring_add(MsgRing* r, const Message* m);
// i = 0 is the oldest kept message
const Message* msgring_get(const MsgRing* r, uint8_t i);
const Message* msgring_newest(const MsgRing* r);
void msgring_scroll(MsgRing* r, int dir, uint8_t rows);

// Draws up to `rows` messages ending at the newest (less scroll), one per 9 px row starting at
// baseline `y`. With `start_ms` set, each line starts with the time since then.
void msgring_draw(
    Canvas* canvas,
    const MsgRing* r,
    int32_t y,
    uint8_t rows,
    bool hex,
    bool show_time,
    uint32_t start_ms);
