// Scoring how well a serial setting fits a talking device. No Flipper dependencies.
#pragma once
#include "cmdfile.h"

typedef struct {
    uint32_t bytes, printable, frame_errors, parity_errors, noise_errors;
} BaudSample;

// 0-100: 70% from how few framing, parity and noise errors there were, 30% from how much of
// what arrived is readable text. Under 40 is unlikely.
uint8_t baud_score(const BaudSample* s);

// The settings tried, in order: all eight rates at 8-N-1, then 7-E-1 and 8-E-1 at 9600,
// 19200, 38400 and 115200.
extern const SerialConfig baud_candidates[];
extern const size_t baud_candidate_count;
