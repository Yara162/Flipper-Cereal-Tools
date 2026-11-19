// The RS-232 port on the Flip'n Cereal HAT: the Flipper's USART on pins 13 (TX) and 14 (RX).
#pragma once
#include <furi.h>
#include <furi_hal.h>
#include "core/cmdfile.h"

// Change this one line if the HAT turns out to use the LPUART (pins 15 and 16).
#define CEREAL_SERIAL_ID FuriHalSerialIdUsart

#define SERIAL_ERR_FRAME 1
#define SERIAL_ERR_PARITY 2
#define SERIAL_ERR_NOISE 4
#define SERIAL_ERR_OVERRUN 8

typedef struct __attribute__((packed)) {
    uint8_t byte;
    uint8_t errors; // SERIAL_ERR_* seen with or just before this byte
    uint32_t at_ms;
} SerialByte;

// How many received bytes a tool takes per screen tick before letting the screen redraw
#define SERIAL_DRAIN_MAX 2048

typedef struct {
    uint32_t frame, parity, noise, overrun, dropped;
} SerialErrors;

typedef struct SerialPort SerialPort;

// Takes the port from the expansion-module service. NULL if another app holds it.
SerialPort* serial_port_open(const SerialConfig* cfg);
void serial_port_close(SerialPort* p);
void serial_port_configure(SerialPort* p, const SerialConfig* cfg);
void serial_port_write(SerialPort* p, const uint8_t* data, size_t len);
// Takes up to `max` received bytes, oldest first. Loop until it returns 0 to empty the queue.
size_t serial_port_read(SerialPort* p, SerialByte* out, size_t max);
// Discards everything received so far.
void serial_port_flush(SerialPort* p);
void serial_port_errors(SerialPort* p, SerialErrors* out, bool reset);
