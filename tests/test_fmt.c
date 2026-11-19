#include "check.h"
#include "fmt.h"

static void test_fmt_text_escapes(void) {
    const uint8_t in[] = {0x00, 0x41, 0x0D, 0x0A, 0xFF, 0x5C, 0x09};
    char out[64];
    size_t n = fmt_text(in, sizeof(in), out, sizeof(out));
    CHECK(strcmp(out, "\\x00A\\r\\n\\xFF\\\\\\t") == 0);
    CHECK(n == strlen(out));
}

static void test_fmt_small_buffer(void) {
    const uint8_t in[] = {0x41, 0x42, 0xFF, 0x43};
    char out[6];
    memset(out, '#', sizeof(out));
    size_t n = fmt_text(in, sizeof(in), out, sizeof(out));
    CHECK(strcmp(out, "AB") == 0); // \xFF would not fit whole, so it stops before it
    CHECK(n == 2);
    char tiny[1];
    CHECK(fmt_text(in, sizeof(in), tiny, sizeof(tiny)) == 0 && tiny[0] == 0);
}

static void test_fmt_hex(void) {
    const uint8_t in[] = {0xAA, 0x11, 0x01};
    char out[16];
    CHECK(fmt_hex(in, sizeof(in), out, sizeof(out)) == 8);
    CHECK(strcmp(out, "AA 11 01") == 0);
    char small[6];
    fmt_hex(in, sizeof(in), small, sizeof(small));
    CHECK(strcmp(small, "AA 11") == 0);
    CHECK(fmt_hex(in, 0, out, sizeof(out)) == 0 && out[0] == 0);
}

int main(void) {
    RUN(test_fmt_text_escapes);
    RUN(test_fmt_small_buffer);
    RUN(test_fmt_hex);
    DONE("test_fmt");
}
