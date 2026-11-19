#include "serial_port.h"
#include <expansion/expansion.h>

#define RX_QUEUE_BYTES 2048

struct SerialPort {
    FuriHalSerialHandle* handle;
    Expansion* expansion;
    FuriStreamBuffer* rx;
    volatile uint8_t pending_errors;
    volatile SerialErrors errors;
};

static void rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SerialPort* p = context;
    if(event & FuriHalSerialRxEventFrameError) {
        p->pending_errors |= SERIAL_ERR_FRAME;
        p->errors.frame++;
    }
    if(event & FuriHalSerialRxEventParityError) {
        p->pending_errors |= SERIAL_ERR_PARITY;
        p->errors.parity++;
    }
    if(event & FuriHalSerialRxEventNoiseError) {
        p->pending_errors |= SERIAL_ERR_NOISE;
        p->errors.noise++;
    }
    if(event & FuriHalSerialRxEventOverrunError) {
        p->pending_errors |= SERIAL_ERR_OVERRUN;
        p->errors.overrun++;
    }
    if(event & FuriHalSerialRxEventData) {
        SerialByte b = {
            .byte = furi_hal_serial_async_rx(handle),
            .errors = p->pending_errors,
            .at_ms = furi_get_tick(),
        };
        p->pending_errors = 0;
        if(furi_stream_buffer_send(p->rx, &b, sizeof(b), 0) != sizeof(b)) p->errors.dropped++;
    }
}

static void apply(SerialPort* p, const SerialConfig* cfg) {
    furi_hal_serial_set_br(p->handle, cfg->baud);
    furi_hal_serial_configure_framing(
        p->handle,
        cfg->data_bits == 7 ? FuriHalSerialDataBits7 : FuriHalSerialDataBits8,
        cfg->parity == 'E' ? FuriHalSerialParityEven :
        cfg->parity == 'O' ? FuriHalSerialParityOdd :
                             FuriHalSerialParityNone,
        cfg->stop_bits == 2 ? FuriHalSerialStopBits2 : FuriHalSerialStopBits1);
}

SerialPort* serial_port_open(const SerialConfig* cfg) {
    SerialPort* p = malloc(sizeof(SerialPort));
    memset(p, 0, sizeof(SerialPort));
    p->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(p->expansion);
    p->handle = furi_hal_serial_control_acquire(CEREAL_SERIAL_ID);
    if(!p->handle) {
        expansion_enable(p->expansion);
        furi_record_close(RECORD_EXPANSION);
        free(p);
        return NULL;
    }
    p->rx = furi_stream_buffer_alloc(RX_QUEUE_BYTES * sizeof(SerialByte), sizeof(SerialByte));
    furi_hal_serial_init(p->handle, cfg->baud);
    apply(p, cfg);
    furi_hal_serial_async_rx_start(p->handle, rx_callback, p, true);
    return p;
}

void serial_port_close(SerialPort* p) {
    if(!p) return;
    furi_hal_serial_tx_wait_complete(p->handle);
    furi_hal_serial_async_rx_stop(p->handle);
    furi_hal_serial_deinit(p->handle);
    furi_hal_serial_control_release(p->handle);
    furi_stream_buffer_free(p->rx);
    expansion_enable(p->expansion);
    furi_record_close(RECORD_EXPANSION);
    free(p);
}

void serial_port_configure(SerialPort* p, const SerialConfig* cfg) {
    furi_hal_serial_tx_wait_complete(p->handle);
    furi_hal_serial_async_rx_stop(p->handle);
    apply(p, cfg);
    furi_hal_serial_async_rx_start(p->handle, rx_callback, p, true);
    serial_port_flush(p);
}

void serial_port_write(SerialPort* p, const uint8_t* data, size_t len) {
    if(len) furi_hal_serial_tx(p->handle, data, len);
}

size_t serial_port_read(SerialPort* p, SerialByte* out, size_t max) {
    size_t got = furi_stream_buffer_receive(p->rx, out, max * sizeof(SerialByte), 0);
    return got / sizeof(SerialByte);
}

void serial_port_flush(SerialPort* p) {
    furi_stream_buffer_reset(p->rx);
}

void serial_port_errors(SerialPort* p, SerialErrors* out, bool reset) {
    FURI_CRITICAL_ENTER();
    out->frame = p->errors.frame;
    out->parity = p->errors.parity;
    out->noise = p->errors.noise;
    out->overrun = p->errors.overrun;
    out->dropped = p->errors.dropped;
    if(reset) {
        p->errors.frame = p->errors.parity = p->errors.noise = 0;
        p->errors.overrun = p->errors.dropped = 0;
    }
    FURI_CRITICAL_EXIT();
}
