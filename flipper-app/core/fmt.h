// Bytes to readable text. No Flipper dependencies.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Printable ASCII as is; \r \n \t \\ escaped; anything else as \xNN. Always NUL-terminated,
// never writes past out_size, never splits an escape. Returns the length written.
size_t fmt_text(const uint8_t* data, size_t len, char* out, size_t out_size);

// "AA 11 01". Always NUL-terminated, never splits a byte. Returns the length written.
size_t fmt_hex(const uint8_t* data, size_t len, char* out, size_t out_size);
