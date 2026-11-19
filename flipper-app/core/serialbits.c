#include "serialbits.h"

uint8_t serial_data_mask(const SerialConfig* cfg) {
    return cfg->data_bits == 7 ? 0x7F : 0xFF;
}
