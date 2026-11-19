// Splits a stream of received bytes into messages: at a line ending, after a quiet gap, or
// when a message fills the buffer. No Flipper dependencies.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MSG_MAX 256

typedef struct {
    uint8_t data[MSG_MAX];
    uint16_t len;
    uint32_t at_ms; // time of the first byte
    bool truncated; // the message continued past MSG_MAX
} Message;

typedef struct {
    Message cur;
    uint32_t last_ms;
    uint32_t gap_ms;
    bool pending_cr; // the message ended in \r; a \n right after belongs to it
    bool complete; // the message is finished and waits to be handed back
} MsgSplitter;

void msgsplit_init(MsgSplitter* s, uint32_t gap_ms);

// Adds one byte. Returns true and fills `out` when a message is complete.
bool msgsplit_feed(MsgSplitter* s, uint8_t byte, uint32_t now_ms, Message* out);

// Call regularly. Returns true and fills `out` when the line has been quiet for the gap.
bool msgsplit_poll(MsgSplitter* s, uint32_t now_ms, Message* out);

uint32_t msgsplit_gap_for_baud(uint32_t baud);
