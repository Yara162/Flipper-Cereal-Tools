#include "check.h"
#include "visca.h"

#define BUILD(...)                                                          \
    do {                                                                    \
        n = visca_build(__VA_ARGS__, out);                                  \
    } while(0)
#define EXPECT(...)                                                         \
    do {                                                                    \
        const uint8_t _w[] = {__VA_ARGS__};                                 \
        CHECK_EQ_BYTES(out, n, _w, sizeof(_w));                             \
    } while(0)

static uint8_t out[32];
static size_t n;

static void test_build_commands(void) {
    BUILD(ViscaPowerOn, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x00, 0x02, 0xFF);
    BUILD(ViscaPowerOff, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x00, 0x03, 0xFF);
    BUILD(ViscaPanTiltUp, 1, 8, 5);
    EXPECT(0x81, 0x01, 0x06, 0x01, 0x08, 0x05, 0x03, 0x01, 0xFF);
    BUILD(ViscaPanTiltDownLeft, 1, 8, 5);
    EXPECT(0x81, 0x01, 0x06, 0x01, 0x08, 0x05, 0x01, 0x02, 0xFF);
    BUILD(ViscaPanTiltStop, 1, 8, 5);
    EXPECT(0x81, 0x01, 0x06, 0x01, 0x08, 0x05, 0x03, 0x03, 0xFF);
    BUILD(ViscaPanTiltHome, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x06, 0x04, 0xFF);
    BUILD(ViscaZoomTele, 1, 4, 0);
    EXPECT(0x81, 0x01, 0x04, 0x07, 0x24, 0xFF);
    BUILD(ViscaZoomWide, 1, 4, 0);
    EXPECT(0x81, 0x01, 0x04, 0x07, 0x34, 0xFF);
    BUILD(ViscaZoomStop, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x07, 0x00, 0xFF);
    BUILD(ViscaFocusAuto, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x38, 0x02, 0xFF);
    BUILD(ViscaFocusOnePush, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x18, 0x01, 0xFF);
    BUILD(ViscaFocusNear, 1, 0, 0);
    EXPECT(0x81, 0x01, 0x04, 0x08, 0x03, 0xFF);
    BUILD(ViscaPresetRecall, 1, 3, 0);
    EXPECT(0x81, 0x01, 0x04, 0x3F, 0x02, 0x03, 0xFF);
    BUILD(ViscaPresetSet, 1, 3, 0);
    EXPECT(0x81, 0x01, 0x04, 0x3F, 0x01, 0x03, 0xFF);
    BUILD(ViscaAddressSet, 1, 0, 0);
    EXPECT(0x88, 0x30, 0x01, 0xFF);
    BUILD(ViscaInqPower, 1, 0, 0);
    EXPECT(0x81, 0x09, 0x04, 0x00, 0xFF);
    BUILD(ViscaInqZoom, 2, 0, 0);
    EXPECT(0x82, 0x09, 0x04, 0x47, 0xFF);
}

static void test_speeds_clamped(void) {
    BUILD(ViscaPanTiltRight, 1, 99, 0);
    CHECK(out[4] == 0x18 && out[5] == 0x01); // pan max 0x18, tilt min 1
    BUILD(ViscaZoomTele, 1, 9, 0);
    CHECK(out[4] == 0x27);
}

static void describe(const uint8_t* pkt, uint8_t len, const char* want) {
    char buf[48];
    visca_describe_reply(pkt, len, buf, sizeof(buf));
    if(strcmp(buf, want) != 0) {
        printf("describe: got \"%s\", want \"%s\"\n", buf, want);
        check_failures++;
    }
}

static void test_describe_replies(void) {
    describe((const uint8_t[]){0x90, 0x41, 0xFF}, 3, "ACK");
    describe((const uint8_t[]){0x90, 0x51, 0xFF}, 3, "Done");
    describe((const uint8_t[]){0x90, 0x60, 0x02, 0xFF}, 4, "Error: syntax");
    describe((const uint8_t[]){0x90, 0x60, 0x03, 0xFF}, 4, "Error: buffer full");
    describe((const uint8_t[]){0x90, 0x61, 0x04, 0xFF}, 4, "Error: cancelled");
    describe((const uint8_t[]){0x90, 0x61, 0x05, 0xFF}, 4, "Error: no socket");
    describe((const uint8_t[]){0x90, 0x61, 0x41, 0xFF}, 4, "Error: not executable");
    describe((const uint8_t[]){0x90, 0x50, 0x02, 0xFF}, 4, "Power: on");
    describe((const uint8_t[]){0x90, 0x50, 0x03, 0xFF}, 4, "Power: off");
    describe((const uint8_t[]){0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF}, 7, "Zoom: 0x1234");
    describe((const uint8_t[]){0x88, 0x30, 0x02, 0xFF}, 4, "Address set: 1 camera");
    describe((const uint8_t[]){0x12, 0x34}, 2, "12 34");
}

static void test_framer(void) {
    ViscaFramer f;
    memset(&f, 0, sizeof(f));
    uint8_t pkt[VISCA_MAX_PACKET], len, got = 0;
    const uint8_t stream[] = {0x90, 0x41, 0xFF, 0x90, 0x51, 0xFF};
    for(size_t i = 0; i < sizeof(stream); i++)
        if(visca_framer_feed(&f, stream[i], pkt, &len)) {
            got++;
            CHECK(len == 3 && pkt[2] == 0xFF);
        }
    CHECK(got == 2);
    got = 0;
    for(int i = 0; i < 20; i++)
        if(visca_framer_feed(&f, 0x11, pkt, &len)) got++;
    const uint8_t ok[] = {0x90, 0x41, 0xFF};
    // the junk run ends at the first FF, which is discarded with it; the next packet is clean
    if(visca_framer_feed(&f, 0xFF, pkt, &len)) got++;
    for(size_t i = 0; i < 3; i++)
        if(visca_framer_feed(&f, ok[i], pkt, &len)) {
            got++;
            CHECK_EQ_BYTES(pkt, len, ok, 3);
        }
    CHECK(got == 1);
}

static ViscaCamera cam;
static uint8_t reply[32];
static char desc[48];

static size_t handle(const uint8_t* pkt, uint8_t len) {
    return visca_camera_handle(&cam, pkt, len, reply, sizeof(reply), desc, sizeof(desc));
}

static void test_camera_commands(void) {
    visca_camera_init(&cam);
    size_t r = handle((const uint8_t[]){0x81, 0x01, 0x04, 0x00, 0x02, 0xFF}, 6);
    const uint8_t ack_done[] = {0x90, 0x41, 0xFF, 0x90, 0x51, 0xFF};
    CHECK_EQ_BYTES(reply, r, ack_done, sizeof(ack_done));
    CHECK(strcmp(desc, "Power on") == 0);
    CHECK(cam.power);
    handle((const uint8_t[]){0x81, 0x01, 0x06, 0x01, 0x08, 0x05, 0x03, 0x01, 0xFF}, 9);
    CHECK(strcmp(desc, "Pan/tilt up, speed 8/5") == 0);
    handle((const uint8_t[]){0x81, 0x01, 0x04, 0x3F, 0x02, 0x03, 0xFF}, 7);
    CHECK(strcmp(desc, "Preset recall 3") == 0);
    handle((const uint8_t[]){0x81, 0x01, 0x04, 0x07, 0x24, 0xFF}, 6);
    CHECK(strcmp(desc, "Zoom tele, speed 4") == 0);
    CHECK(cam.received == 4);
}

static void test_camera_inquiries(void) {
    visca_camera_init(&cam);
    cam.power = true;
    size_t r = handle((const uint8_t[]){0x81, 0x09, 0x04, 0x00, 0xFF}, 5);
    const uint8_t on[] = {0x90, 0x50, 0x02, 0xFF};
    CHECK_EQ_BYTES(reply, r, on, sizeof(on));
    cam.power = false;
    r = handle((const uint8_t[]){0x81, 0x09, 0x04, 0x00, 0xFF}, 5);
    const uint8_t off[] = {0x90, 0x50, 0x03, 0xFF};
    CHECK_EQ_BYTES(reply, r, off, sizeof(off));
    cam.zoom = 0x1234;
    r = handle((const uint8_t[]){0x81, 0x09, 0x04, 0x47, 0xFF}, 5);
    const uint8_t zoom[] = {0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF};
    CHECK_EQ_BYTES(reply, r, zoom, sizeof(zoom));
}

static void test_camera_address_and_unknown(void) {
    visca_camera_init(&cam);
    size_t r = handle((const uint8_t[]){0x88, 0x30, 0x01, 0xFF}, 4);
    const uint8_t next[] = {0x88, 0x30, 0x02, 0xFF};
    CHECK_EQ_BYTES(reply, r, next, sizeof(next));
    CHECK(cam.address == 1);
    r = handle((const uint8_t[]){0x81, 0x01, 0x7E, 0xFF}, 4);
    const uint8_t syntax[] = {0x90, 0x60, 0x02, 0xFF};
    CHECK_EQ_BYTES(reply, r, syntax, sizeof(syntax));
    CHECK(strcmp(desc, "Unknown: 81 01 7E FF") == 0);
    r = handle((const uint8_t[]){0x82, 0x01, 0x04, 0x00, 0x02, 0xFF}, 6);
    CHECK(r == 0);
    CHECK(strcmp(desc, "For camera 2 (ignored)") == 0);
}

static void test_camera_follows_address_set(void) {
    visca_camera_init(&cam);
    size_t r = handle((const uint8_t[]){0x88, 0x30, 0x03, 0xFF}, 4); // controller numbers us 3
    CHECK(cam.address == 3);
    const uint8_t next[] = {0x88, 0x30, 0x04, 0xFF};
    CHECK_EQ_BYTES(reply, r, next, sizeof(next));
    r = handle((const uint8_t[]){0x83, 0x01, 0x04, 0x00, 0x03, 0xFF}, 6);
    const uint8_t ack_done[] = {0xB0, 0x41, 0xFF, 0xB0, 0x51, 0xFF};
    CHECK_EQ_BYTES(reply, r, ack_done, sizeof(ack_done));
}

int main(void) {
    RUN(test_build_commands);
    RUN(test_speeds_clamped);
    RUN(test_describe_replies);
    RUN(test_framer);
    RUN(test_camera_commands);
    RUN(test_camera_inquiries);
    RUN(test_camera_address_and_unknown);
    RUN(test_camera_follows_address_set);
    DONE("test_visca");
}
