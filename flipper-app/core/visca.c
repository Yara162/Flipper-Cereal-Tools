#include "visca.h"
#include <stdio.h>
#include <string.h>

bool visca_framer_feed(ViscaFramer* f, uint8_t byte, uint8_t* packet, uint8_t* len) {
    if(f->overflow) {
        if(byte == 0xFF) {
            f->overflow = false;
            f->len = 0;
        }
        return false;
    }
    if(f->len >= VISCA_MAX_PACKET) {
        f->overflow = true;
        f->len = 0;
        return false;
    }
    f->buf[f->len++] = byte;
    if(byte != 0xFF) return false;
    memcpy(packet, f->buf, f->len);
    *len = f->len;
    f->len = 0;
    return true;
}

static uint8_t clamp(uint8_t v, uint8_t lo, uint8_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

size_t visca_build(ViscaCmd cmd, uint8_t addr, uint8_t arg1, uint8_t arg2, uint8_t* out) {
    uint8_t a = (uint8_t)(0x80 | (addr & 0x07));
    size_t n = 0;
    static const uint8_t dirs[][2] = {
        [ViscaPanTiltUp] = {0x03, 0x01},
        [ViscaPanTiltDown] = {0x03, 0x02},
        [ViscaPanTiltLeft] = {0x01, 0x03},
        [ViscaPanTiltRight] = {0x02, 0x03},
        [ViscaPanTiltUpLeft] = {0x01, 0x01},
        [ViscaPanTiltUpRight] = {0x02, 0x01},
        [ViscaPanTiltDownLeft] = {0x01, 0x02},
        [ViscaPanTiltDownRight] = {0x02, 0x02},
        [ViscaPanTiltStop] = {0x03, 0x03},
    };
    switch(cmd) {
    case ViscaPowerOn:
    case ViscaPowerOff:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x00;
        out[n++] = cmd == ViscaPowerOn ? 0x02 : 0x03;
        break;
    case ViscaPanTiltUp:
    case ViscaPanTiltDown:
    case ViscaPanTiltLeft:
    case ViscaPanTiltRight:
    case ViscaPanTiltUpLeft:
    case ViscaPanTiltUpRight:
    case ViscaPanTiltDownLeft:
    case ViscaPanTiltDownRight:
    case ViscaPanTiltStop:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x06, out[n++] = 0x01;
        out[n++] = clamp(arg1, 0x01, 0x18);
        out[n++] = clamp(arg2, 0x01, 0x14);
        out[n++] = dirs[cmd][0];
        out[n++] = dirs[cmd][1];
        break;
    case ViscaPanTiltHome:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x06, out[n++] = 0x04;
        break;
    case ViscaZoomTele:
    case ViscaZoomWide:
    case ViscaZoomStop:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x07;
        out[n++] = cmd == ViscaZoomStop ? 0x00 :
                                          (uint8_t)((cmd == ViscaZoomTele ? 0x20 : 0x30) | clamp(arg1, 0, 7));
        break;
    case ViscaFocusAuto:
    case ViscaFocusManual:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x38;
        out[n++] = cmd == ViscaFocusAuto ? 0x02 : 0x03;
        break;
    case ViscaFocusOnePush:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x18, out[n++] = 0x01;
        break;
    case ViscaFocusNear:
    case ViscaFocusFar:
    case ViscaFocusStop:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x08;
        out[n++] = cmd == ViscaFocusNear ? 0x03 : cmd == ViscaFocusFar ? 0x02 : 0x00;
        break;
    case ViscaPresetRecall:
    case ViscaPresetSet:
        out[n++] = a, out[n++] = 0x01, out[n++] = 0x04, out[n++] = 0x3F;
        out[n++] = cmd == ViscaPresetRecall ? 0x02 : 0x01;
        out[n++] = clamp(arg1, 0, 0x7F);
        break;
    case ViscaAddressSet:
        out[n++] = 0x88, out[n++] = 0x30, out[n++] = 0x01;
        break;
    case ViscaInqPower:
        out[n++] = a, out[n++] = 0x09, out[n++] = 0x04, out[n++] = 0x00;
        break;
    case ViscaInqZoom:
        out[n++] = a, out[n++] = 0x09, out[n++] = 0x04, out[n++] = 0x47;
        break;
    }
    out[n++] = 0xFF;
    return n;
}

static void hex(const uint8_t* p, uint8_t len, char* out, size_t size) {
    size_t n = 0;
    if(size) out[0] = 0;
    for(uint8_t i = 0; i < len && n + 4 <= size; i++) n += (size_t)snprintf(out + n, size - n, i ? " %02X" : "%02X", p[i]);
}

void visca_describe_reply(const uint8_t* p, uint8_t len, char* out, size_t size) {
    if(len == 4 && p[0] == 0x88 && p[1] == 0x30) {
        snprintf(out, size, "Address set: %u camera%s", p[2] - 1, p[2] == 2 ? "" : "s");
        return;
    }
    if(len >= 3 && (p[0] & 0x8F) == 0x80 && p[len - 1] == 0xFF) {
        uint8_t kind = p[1] & 0xF0;
        if(len == 3 && kind == 0x40) {
            snprintf(out, size, "ACK");
            return;
        }
        if(len == 3 && kind == 0x50) {
            snprintf(out, size, "Done");
            return;
        }
        if(len == 4 && kind == 0x60) {
            const char* e = p[2] == 0x01 ? "message length" :
                            p[2] == 0x02 ? "syntax" :
                            p[2] == 0x03 ? "buffer full" :
                            p[2] == 0x04 ? "cancelled" :
                            p[2] == 0x05 ? "no socket" :
                            p[2] == 0x41 ? "not executable" :
                                           NULL;
            if(e) {
                snprintf(out, size, "Error: %s", e);
                return;
            }
        }
        if(kind == 0x50 && len == 4 && (p[2] == 0x02 || p[2] == 0x03)) {
            snprintf(out, size, "Power: %s", p[2] == 0x02 ? "on" : "off");
            return;
        }
        if(kind == 0x50 && len == 7) {
            unsigned v = (unsigned)((p[2] & 0x0F) << 12 | (p[3] & 0x0F) << 8 | (p[4] & 0x0F) << 4 | (p[5] & 0x0F));
            snprintf(out, size, "Zoom: 0x%04X", v);
            return;
        }
    }
    hex(p, len, out, size);
}

void visca_camera_init(ViscaCamera* c) {
    memset(c, 0, sizeof(*c));
    c->address = 1;
    c->power = true;
    c->focus_auto = true;
}

static const char* direction(uint8_t y, uint8_t z) {
    if(y == 0x03 && z == 0x01) return "up";
    if(y == 0x03 && z == 0x02) return "down";
    if(y == 0x01 && z == 0x03) return "left";
    if(y == 0x02 && z == 0x03) return "right";
    if(y == 0x01 && z == 0x01) return "up-left";
    if(y == 0x02 && z == 0x01) return "up-right";
    if(y == 0x01 && z == 0x02) return "down-left";
    if(y == 0x02 && z == 0x02) return "down-right";
    if(y == 0x03 && z == 0x03) return "stop";
    return NULL;
}

static size_t put(uint8_t* reply, size_t max, size_t n, const uint8_t* bytes, size_t len) {
    if(n + len > max) return n;
    memcpy(reply + n, bytes, len);
    return n + len;
}

size_t visca_camera_handle(
    ViscaCamera* c,
    const uint8_t* p,
    uint8_t len,
    uint8_t* reply,
    size_t reply_max,
    char* desc,
    size_t desc_size) {
    char raw[3 * VISCA_MAX_PACKET + 1];
    hex(p, len, raw, sizeof(raw));
    if(len < 3 || p[len - 1] != 0xFF || !(p[0] & 0x80)) {
        snprintf(desc, desc_size, "Bad packet: %s", raw);
        return 0;
    }
    // broadcast: address set and interface clear
    if(p[0] == 0x88) {
        c->received++;
        if(len == 4 && p[1] == 0x30) {
            c->address = (uint8_t)(p[2] >= 1 && p[2] <= 7 ? p[2] : 1);
            snprintf(desc, desc_size, "Address set: I am camera %u", c->address);
            uint8_t next[] = {0x88, 0x30, (uint8_t)(c->address + 1), 0xFF};
            return put(reply, reply_max, 0, next, sizeof(next));
        }
        if(len == 5 && p[1] == 0x01 && p[2] == 0x00 && p[3] == 0x01) {
            snprintf(desc, desc_size, "Interface clear");
            return put(reply, reply_max, 0, p, len);
        }
        snprintf(desc, desc_size, "Broadcast: %s", raw);
        return 0;
    }
    uint8_t target = p[0] & 0x07;
    if(target != c->address) {
        snprintf(desc, desc_size, "For camera %u (ignored)", target);
        return 0;
    }
    c->received++;
    uint8_t y = (uint8_t)(0x80 | (c->address << 4));
    uint8_t ack_done[] = {y, 0x41, 0xFF, y, 0x51, 0xFF};
    uint8_t syntax[] = {y, 0x60, 0x02, 0xFF};
    bool known = false;

    if(p[1] == 0x01) { // commands
        if(len == 6 && p[2] == 0x04 && p[3] == 0x00 && (p[4] == 0x02 || p[4] == 0x03)) {
            c->power = p[4] == 0x02;
            snprintf(desc, desc_size, "Power %s", c->power ? "on" : "off");
            known = true;
        } else if(len == 9 && p[2] == 0x06 && p[3] == 0x01 && direction(p[6], p[7])) {
            const char* d = direction(p[6], p[7]);
            if(strcmp(d, "stop") == 0)
                snprintf(desc, desc_size, "Pan/tilt stop");
            else
                snprintf(desc, desc_size, "Pan/tilt %s, speed %u/%u", d, p[4], p[5]);
            known = true;
        } else if(len == 5 && p[2] == 0x06 && (p[3] == 0x04 || p[3] == 0x05)) {
            c->pan = c->tilt = 0;
            snprintf(desc, desc_size, p[3] == 0x04 ? "Home" : "Pan/tilt reset");
            known = true;
        } else if(len == 6 && p[2] == 0x04 && p[3] == 0x07) {
            uint8_t v = p[4];
            if(v == 0x00)
                snprintf(desc, desc_size, "Zoom stop");
            else if(v == 0x02 || v == 0x03)
                snprintf(desc, desc_size, "Zoom %s", v == 0x02 ? "tele" : "wide");
            else if((v & 0xF0) == 0x20 || (v & 0xF0) == 0x30)
                snprintf(desc, desc_size, "Zoom %s, speed %u", (v & 0xF0) == 0x20 ? "tele" : "wide", v & 0x0F);
            else
                goto unknown;
            if(v == 0x02 || (v & 0xF0) == 0x20) c->zoom = (uint16_t)(c->zoom > 0x3C00 ? 0x4000 : c->zoom + 0x0400);
            if(v == 0x03 || (v & 0xF0) == 0x30) c->zoom = (uint16_t)(c->zoom < 0x0400 ? 0 : c->zoom - 0x0400);
            known = true;
        } else if(len == 9 && p[2] == 0x04 && p[3] == 0x47) {
            c->zoom = (uint16_t)((p[4] & 0x0F) << 12 | (p[5] & 0x0F) << 8 | (p[6] & 0x0F) << 4 | (p[7] & 0x0F));
            snprintf(desc, desc_size, "Zoom to 0x%04X", c->zoom);
            known = true;
        } else if(len == 6 && p[2] == 0x04 && p[3] == 0x38 && (p[4] == 0x02 || p[4] == 0x03 || p[4] == 0x10)) {
            c->focus_auto = p[4] == 0x02 ? true : p[4] == 0x03 ? false : !c->focus_auto;
            snprintf(desc, desc_size, "Focus %s", c->focus_auto ? "auto" : "manual");
            known = true;
        } else if(len == 6 && p[2] == 0x04 && p[3] == 0x18 && p[4] == 0x01) {
            snprintf(desc, desc_size, "Focus one push");
            known = true;
        } else if(len == 6 && p[2] == 0x04 && p[3] == 0x08) {
            uint8_t v = p[4];
            if(v == 0x00)
                snprintf(desc, desc_size, "Focus stop");
            else if(v == 0x02 || v == 0x03)
                snprintf(desc, desc_size, "Focus %s", v == 0x02 ? "far" : "near");
            else if((v & 0xF0) == 0x20 || (v & 0xF0) == 0x30)
                snprintf(desc, desc_size, "Focus %s, speed %u", (v & 0xF0) == 0x20 ? "far" : "near", v & 0x0F);
            else
                goto unknown;
            known = true;
        } else if(len == 7 && p[2] == 0x04 && p[3] == 0x3F && p[4] <= 0x02) {
            const char* what = p[4] == 0x02 ? "recall" : p[4] == 0x01 ? "set" : "reset";
            snprintf(desc, desc_size, "Preset %s %u", what, p[5]);
            known = true;
        }
        if(known) return put(reply, reply_max, 0, ack_done, sizeof(ack_done));
    } else if(p[1] == 0x09 && len == 5) { // inquiries
        if(p[2] == 0x04 && p[3] == 0x00) {
            snprintf(desc, desc_size, "Asks: power");
            uint8_t r[] = {y, 0x50, (uint8_t)(c->power ? 0x02 : 0x03), 0xFF};
            return put(reply, reply_max, 0, r, sizeof(r));
        }
        if(p[2] == 0x04 && p[3] == 0x47) {
            snprintf(desc, desc_size, "Asks: zoom position");
            uint8_t r[] = {y,
                           0x50,
                           (uint8_t)(c->zoom >> 12 & 0x0F),
                           (uint8_t)(c->zoom >> 8 & 0x0F),
                           (uint8_t)(c->zoom >> 4 & 0x0F),
                           (uint8_t)(c->zoom & 0x0F),
                           0xFF};
            return put(reply, reply_max, 0, r, sizeof(r));
        }
        if(p[2] == 0x04 && p[3] == 0x38) {
            snprintf(desc, desc_size, "Asks: focus mode");
            uint8_t r[] = {y, 0x50, (uint8_t)(c->focus_auto ? 0x02 : 0x03), 0xFF};
            return put(reply, reply_max, 0, r, sizeof(r));
        }
        if(p[2] == 0x06 && p[3] == 0x12) {
            snprintf(desc, desc_size, "Asks: pan/tilt position");
            uint16_t pan = (uint16_t)c->pan, tilt = (uint16_t)c->tilt;
            uint8_t r[] = {y,
                           0x50,
                           (uint8_t)(pan >> 12 & 0x0F),
                           (uint8_t)(pan >> 8 & 0x0F),
                           (uint8_t)(pan >> 4 & 0x0F),
                           (uint8_t)(pan & 0x0F),
                           (uint8_t)(tilt >> 12 & 0x0F),
                           (uint8_t)(tilt >> 8 & 0x0F),
                           (uint8_t)(tilt >> 4 & 0x0F),
                           (uint8_t)(tilt & 0x0F),
                           0xFF};
            return put(reply, reply_max, 0, r, sizeof(r));
        }
    }
unknown:
    snprintf(desc, desc_size, "Unknown: %s", raw);
    return put(reply, reply_max, 0, syntax, sizeof(syntax));
}
