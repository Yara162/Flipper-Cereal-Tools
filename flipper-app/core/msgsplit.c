#include "msgsplit.h"
#include <string.h>

void msgsplit_init(MsgSplitter* s, uint32_t gap_ms) {
    memset(s, 0, sizeof(*s));
    s->gap_ms = gap_ms;
}

uint32_t msgsplit_gap_for_baud(uint32_t baud) {
    if(baud == 0) return 400;
    uint32_t gap = 480000 / baud;
    return gap < 20 ? 20 : gap;
}

static bool release(MsgSplitter* s, Message* out) {
    if(s->cur.len == 0) return false;
    *out = s->cur;
    memset(&s->cur, 0, sizeof(s->cur));
    s->pending_cr = false;
    s->complete = false;
    return true;
}

bool msgsplit_feed(MsgSplitter* s, uint8_t byte, uint32_t now_ms, Message* out) {
    bool released = false;
    if(s->complete || (s->cur.len && now_ms - s->last_ms >= s->gap_ms)) {
        released = release(s, out);
    } else if(s->pending_cr) {
        if(byte == '\n') {
            s->cur.data[s->cur.len++] = byte; // there is always room: a \r ends before MSG_MAX
            s->last_ms = now_ms;
            return release(s, out);
        }
        released = release(s, out);
    }
    if(s->cur.len == 0) s->cur.at_ms = now_ms;
    s->cur.data[s->cur.len++] = byte;
    s->last_ms = now_ms;

    // Only one message can be handed back per byte. A message already released above wins;
    // a message this byte completes is marked and handed back by the next feed or poll.
    if(byte == '\n') {
        if(!released) return release(s, out);
        s->complete = true;
        return true;
    }
    if(byte == '\r') {
        if(s->cur.len < MSG_MAX) {
            s->pending_cr = true;
            return released;
        }
    }
    if(s->cur.len >= MSG_MAX) {
        s->cur.truncated = true;
        if(!released) return release(s, out);
        s->complete = true;
    }
    return released;
}

bool msgsplit_poll(MsgSplitter* s, uint32_t now_ms, Message* out) {
    if(s->complete || (s->cur.len && now_ms - s->last_ms >= s->gap_ms)) return release(s, out);
    return false;
}
