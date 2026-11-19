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

// Something to send at each setting so a device that only answers when asked has something
// to answer. Index 0 is "Off".
typedef struct {
    const char* name;
    const uint8_t* data;
    uint8_t len;
} BaudProbe;

extern const BaudProbe baud_probes[];
extern const size_t baud_probe_count;

// Cable check: the test pattern is sent with the far end's TX and RX joined.
typedef enum { LoopbackNothing, LoopbackPass, LoopbackPartial, LoopbackGarbled } LoopbackVerdict;

extern const uint8_t loopback_pattern[];
extern const size_t loopback_pattern_len;

// `matched` gets how many pattern bytes came back in order.
LoopbackVerdict loopback_judge(const uint8_t* got, size_t got_len, size_t* matched);
