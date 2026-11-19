// Bytes to readable text. No Flipper dependencies.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Printable ASCII as is; \r \n \t \\ escaped; anything else as \xNN. Always NUL-terminated,
// never writes past out_size, never splits an escape. Returns the length written.
size_t fmt_text(const uint8_t* data, size_t len, char* out, size_t out_size);

// "AA 11 01". Always NUL-terminated, never splits a byte. Returns the length written.
size_t fmt_hex(const uint8_t* data, size_t len, char* out, size_t out_size);

#include "msgsplit.h"

// A received message as text or hex, followed by "+" when it continues in the next message.
size_t fmt_message(const Message* m, bool hex, char* out, size_t out_size);

// Splits text over two lines of at most `width` characters, breaking at a space when it can.
void fmt_wrap(const char* text, size_t width, char* line1, char* line2);
