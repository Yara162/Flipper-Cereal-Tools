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
    int score = 100 - (int)(penalty > 100 ? 100 : penalty);
    if((uint64_t)s->printable * 10 > (uint64_t)s->bytes * 9) score += 10;
    if(score > 100) score = 100;
    if(score < 0) score = 0;
    return (uint8_t)score;
}
