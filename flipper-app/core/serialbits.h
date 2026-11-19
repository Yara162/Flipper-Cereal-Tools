// Serial framing helpers. No Flipper dependencies.
#pragma once
#include "cmdfile.h"

// With 7 data bits the UART hands over the parity bit in bit 7; mask it off.
uint8_t serial_data_mask(const SerialConfig* cfg);
