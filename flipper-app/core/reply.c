#include "reply.h"

bool reply_complete(bool matched, size_t rx_len, uint32_t sent_at, uint32_t now, uint32_t last_rx_at) {
    (void)matched; // a match no longer ends the wait early: values often follow the matched prefix
    if(rx_len >= REPLY_MAX) return true;
    if(rx_len && now - last_rx_at >= REPLY_QUIET_MS) return true;
    return now - sent_at >= REPLY_TIMEOUT_MS;
}
