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

static const uint8_t probe_cr[] = {'\r'};
static const uint8_t probe_benq[] = {'\r', '*', 'p', 'o', 'w', '=', '?', '#', '\r'};
static const uint8_t probe_epson[] = {'P', 'W', 'R', '?', '\r'};
static const uint8_t probe_lg[] = {'k', 'a', ' ', '0', '1', ' ', 'F', 'F', '\r'};
static const uint8_t probe_mdc[] = {0xAA, 0x11, 0x01, 0x00, 0x12}; // power status, display ID 1
static const uint8_t probe_sony[] = {0x83, 0x00, 0x00, 0xFF, 0xFF, 0x81}; // power query
static const uint8_t probe_visca[] = {0x81, 0x09, 0x04, 0x00, 0xFF}; // camera 1 power inquiry

const BaudProbe baud_probes[] = {
    {"Off", NULL, 0},
    {"Carriage return", probe_cr, sizeof(probe_cr)},
    {"BenQ status", probe_benq, sizeof(probe_benq)},
    {"Epson status", probe_epson, sizeof(probe_epson)},
    {"LG status", probe_lg, sizeof(probe_lg)},
    {"Samsung MDC status", probe_mdc, sizeof(probe_mdc)},
    {"Sony status", probe_sony, sizeof(probe_sony)},
    {"VISCA power", probe_visca, sizeof(probe_visca)},
};
const size_t baud_probe_count = sizeof(baud_probes) / sizeof(baud_probes[0]);

const uint8_t loopback_pattern[] = "CEREAL CABLE CHECK 0123456789\r\n";
const size_t loopback_pattern_len = sizeof(loopback_pattern) - 1;

LoopbackVerdict loopback_judge(const uint8_t* got, size_t got_len, size_t* matched) {
    size_t best = 0;
    for(size_t start = 0; start < got_len; start++) {
        size_t n = 0;
        while(start + n < got_len && n < loopback_pattern_len && got[start + n] == loopback_pattern[n]) n++;
        if(n > best) best = n;
    }
    *matched = best;
    if(got_len == 0) return LoopbackNothing;
    if(best == loopback_pattern_len) return LoopbackPass;
    if(best >= 4) return LoopbackPartial; // fewer could be coincidence
    return LoopbackGarbled;
}
