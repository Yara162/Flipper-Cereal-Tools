#include "baudscore.h"

const SerialConfig baud_candidates[] = {
    {1200, 8, 'N', 1},  {2400, 8, 'N', 1},  {4800, 8, 'N', 1},   {9600, 8, 'N', 1},
    {19200, 8, 'N', 1}, {38400, 8, 'N', 1}, {57600, 8, 'N', 1},  {115200, 8, 'N', 1},
    {9600, 7, 'E', 1},  {9600, 8, 'E', 1},  {19200, 7, 'E', 1},  {19200, 8, 'E', 1},
    {38400, 7, 'E', 1}, {38400, 8, 'E', 1}, {115200, 7, 'E', 1}, {115200, 8, 'E', 1},
};
const size_t baud_candidate_count = sizeof(baud_candidates) / sizeof(baud_candidates[0]);

uint8_t baud_score(const BaudSample* s) {
    if(s->bytes < 4) return 0;
    uint64_t errors = (uint64_t)s->frame_errors + s->parity_errors + s->noise_errors;
    uint64_t penalty = errors * 400 / s->bytes;
    uint32_t clean = 100 - (uint32_t)(penalty > 100 ? 100 : penalty);
    // A 7-E-1 device heard at 8-N-1 makes no framing errors but about half its bytes have bit 7
    // set, so readable text must count in the score itself, not only as a tie-breaker.
    uint32_t printable = (uint32_t)((uint64_t)s->printable * 100 / s->bytes);
    uint32_t score = (clean * 70 + printable * 30 + 50) / 100;
    return (uint8_t)(score > 100 ? 100 : score);
}
