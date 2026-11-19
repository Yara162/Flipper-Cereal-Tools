// VISCA camera control: packet framing, building controller commands, describing replies, and
// acting as a camera. No Flipper dependencies.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VISCA_MAX_PACKET 16

typedef struct {
    uint8_t buf[VISCA_MAX_PACKET];
    uint8_t len;
    bool overflow; // discarding until the next 0xFF
} ViscaFramer;

// Feeds one byte. Returns true with the whole packet (including the final 0xFF) when one ends.
bool visca_framer_feed(ViscaFramer* f, uint8_t byte, uint8_t* packet, uint8_t* len);

typedef enum {
    ViscaPowerOn,
    ViscaPowerOff,
    ViscaPanTiltUp,
    ViscaPanTiltDown,
    ViscaPanTiltLeft,
    ViscaPanTiltRight,
    ViscaPanTiltUpLeft,
    ViscaPanTiltUpRight,
    ViscaPanTiltDownLeft,
    ViscaPanTiltDownRight,
    ViscaPanTiltStop,
    ViscaPanTiltHome,
    ViscaZoomTele,
    ViscaZoomWide,
    ViscaZoomStop,
    ViscaFocusAuto,
    ViscaFocusManual,
    ViscaFocusOnePush,
    ViscaFocusNear,
    ViscaFocusFar,
    ViscaFocusStop,
    ViscaPresetRecall,
    ViscaPresetSet,
    ViscaAddressSet,
    ViscaInqPower,
    ViscaInqZoom,
} ViscaCmd;

// arg1/arg2: pan and tilt speed for pan/tilt, speed 0-7 for zoom, preset number for presets.
// Returns the packet length.
size_t visca_build(ViscaCmd cmd, uint8_t addr, uint8_t arg1, uint8_t arg2, uint8_t* out);

void visca_describe_reply(const uint8_t* pkt, uint8_t len, char* out, size_t size);

typedef struct {
    uint8_t address; // 1-7
    bool power;
    uint16_t zoom;
    bool focus_auto;
    int16_t pan, tilt;
    uint32_t received;
} ViscaCamera;

void visca_camera_init(ViscaCamera* c);

// Handles one packet sent to the camera. Returns the number of reply bytes written (0 if the
// packet is for another camera) and describes the packet in plain words.
size_t visca_camera_handle(
    ViscaCamera* c,
    const uint8_t* pkt,
    uint8_t len,
    uint8_t* reply,
    size_t reply_max,
    char* desc,
    size_t desc_size);
