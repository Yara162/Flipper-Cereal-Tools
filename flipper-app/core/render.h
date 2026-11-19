// Turning command text into bytes, matching replies, and reading values out of replies.
// No Flipper dependencies, so it can be tested on a PC.
//
// Placeholders: {key} a setting's value (one byte in hex data, decimal in text);
// {key:2d} {key:2x} {key:2X} zero-padded decimal / lower-case hex / upper-case hex, width 1-4
// (text only); {sum} low byte of the sum of all bytes before it; {sum1} the same skipping the
// first byte; {xor} XOR of all bytes before it (hex only). Text escapes: \r \n \t \\ \xNN.
// In expected replies written as hex, ?? matches any byte.
#pragma once
#include "cmdfile.h"

int render_bytes(
    const char* data,
    DataKind kind,
    const CommandSet* set,
    uint8_t* out,
    size_t out_max,
    char* err,
    size_t err_size);

int render_pattern(
    const char* data,
    DataKind kind,
    const CommandSet* set,
    uint8_t* bytes,
    uint8_t* mask,
    size_t max,
    char* err,
    size_t err_size);

bool render_match(
    const uint8_t* reply,
    size_t reply_len,
    const uint8_t* bytes,
    const uint8_t* mask,
    size_t pattern_len);

bool render_show(const ShowRule* rule, const uint8_t* reply, size_t len, char* out, size_t out_size);
