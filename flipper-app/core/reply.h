// When a command's reply is complete. No Flipper dependencies.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define REPLY_MAX 256
#define REPLY_TIMEOUT_MS 1000
#define REPLY_QUIET_MS 150

// Done once the line has been quiet for REPLY_QUIET_MS after data arrived (matched or not, so a
// value after the expected prefix is not cut off), after REPLY_TIMEOUT_MS with nothing, or when
// the buffer is full.
bool reply_complete(bool matched, size_t rx_len, uint32_t sent_at, uint32_t now, uint32_t last_rx_at);
