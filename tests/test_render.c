#include "check.h"
#include "render.h"

static CommandSet set;
static char err[64];

static void with_id(int32_t v) {
    memset(&set, 0, sizeof(set));
    set.setting_count = 1;
    strcpy(set.settings[0].key, "id");
    set.settings[0].min = 0;
    set.settings[0].max = 999;
    set.settings[0].value = v;
}

static int hex(const char* data, uint8_t* out) {
    err[0] = 0;
    return render_bytes(data, DataHex, &set, out, 64, err, sizeof(err));
}

static int text(const char* data, uint8_t* out) {
    err[0] = 0;
    return render_bytes(data, DataText, &set, out, 64, err, sizeof(err));
}

static void test_samsung_mdc_power_on(void) {
    with_id(1);
    uint8_t out[64];
    int n = hex("AA 11 {id} 01 01 {sum1}", out);
    const uint8_t want[] = {0xAA, 0x11, 0x01, 0x01, 0x01, 0x14};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want, sizeof(want));
}

static void test_sony_style_checksum(void) {
    with_id(1);
    uint8_t out[64];
    int n = hex("8C 00 00 02 01 {sum}", out);
    const uint8_t want[] = {0x8C, 0x00, 0x00, 0x02, 0x01, 0x8F};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want, sizeof(want));
}

static void test_xor_checksum(void) {
    with_id(1);
    uint8_t out[64];
    int n = hex("01 03 F0 {xor}", out);
    const uint8_t want[] = {0x01, 0x03, 0xF0, 0xF2};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want, sizeof(want));
}

static void test_hex_case_and_spacing(void) {
    with_id(1);
    uint8_t out[64];
    int n = hex("aa11 Ff", out);
    const uint8_t want[] = {0xAA, 0x11, 0xFF};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want, sizeof(want));
}

static void test_text_with_hex_id(void) {
    with_id(1);
    uint8_t out[64];
    int n = text("ka {id:2x} 01\r", out);
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, "ka 01 01\r", 9);
    with_id(26);
    n = text("{id:2X}|{id:3d}|{id}", out);
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, "1A|026|26", 9);
}

static void test_escapes(void) {
    with_id(1);
    uint8_t out[64];
    int n = text("*pow=on#\r", out);
    const uint8_t want1[] = {0x2A, 0x70, 0x6F, 0x77, 0x3D, 0x6F, 0x6E, 0x23, 0x0D};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want1, sizeof(want1));
    n = text("\\x02PON\\x03", out);
    const uint8_t want2[] = {0x02, 0x50, 0x4F, 0x4E, 0x03};
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, want2, sizeof(want2));
    n = text("a\\\\b\\r\\n\\t", out);
    CHECK_EQ_BYTES(out, n < 0 ? 0 : (size_t)n, "a\\b\r\n\t", 6);
}

static void test_unknown_placeholder_errors(void) {
    with_id(1);
    uint8_t out[64];
    CHECK(hex("AA {idd}", out) == -1);
    CHECK(strstr(err, "idd") != NULL);
    CHECK(text("x{idd}", out) == -1);
}

static void test_value_out_of_byte_range_errors(void) {
    with_id(300);
    uint8_t out[64];
    CHECK(hex("AA {id}", out) == -1);
    CHECK(err[0] != 0);
}

static void test_bad_hex_errors(void) {
    with_id(1);
    uint8_t out[64];
    CHECK(hex("AA 1", out) == -1);
    CHECK(hex("AA ZZ", out) == -1);
    CHECK(hex("AA ??", out) == -1); // wildcards only in expected replies
    CHECK(text("\\xZZ", out) == -1);
    CHECK(hex("{unclosed", out) == -1);
}

static void test_checksum_in_text_errors(void) {
    with_id(1);
    uint8_t out[64];
    CHECK(text("abc{sum}", out) == -1);
}

static void test_pattern_wildcards_and_match(void) {
    with_id(1);
    uint8_t bytes[64], mask[64];
    int n = render_pattern("AA FF ?? 03 41", DataHex, &set, bytes, mask, sizeof(bytes), err, sizeof(err));
    CHECK(n == 5);
    CHECK(mask[2] == 0 && mask[0] == 0xFF);
    const uint8_t ok[] = {0xAA, 0xFF, 0x05, 0x03, 0x41, 0x11, 0x01, 0x5A};
    const uint8_t bad[] = {0xAA, 0xFF, 0x05, 0x03, 0x4E};
    CHECK(render_match(ok, sizeof(ok), bytes, mask, (size_t)n));
    CHECK(!render_match(bad, sizeof(bad), bytes, mask, (size_t)n));
    const uint8_t later[] = {0x0D, 0x0A, 0xAA, 0xFF, 0x01, 0x03, 0x41};
    CHECK(render_match(later, sizeof(later), bytes, mask, (size_t)n));
    CHECK(!render_match(ok, 3, bytes, mask, (size_t)n));
}

static void test_text_pattern(void) {
    with_id(1);
    uint8_t bytes[64], mask[64];
    int n = render_pattern("a {id:2x} OK", DataText, &set, bytes, mask, sizeof(bytes), err, sizeof(err));
    CHECK(n == 7);
    CHECK(render_match((const uint8_t*)"a 01 OK01x", 10, bytes, mask, (size_t)n));
}

static ShowRule text_rule(const char* after, const char* until) {
    ShowRule r;
    memset(&r, 0, sizeof(r));
    strcpy(r.after, after);
    strcpy(r.until, until);
    return r;
}

static void test_show_text_rule(void) {
    ShowRule r = text_rule("LAMP=", "\r");
    char out[32];
    const char* reply = "LAMP=01234\r:";
    CHECK(render_show(&r, (const uint8_t*)reply, strlen(reply), out, sizeof(out)));
    CHECK(strcmp(out, "01234") == 0);
    // no until: value runs to the end of the reply
    ShowRule r2 = text_rule("POW=", "");
    const char* reply2 = "*POW=ON#";
    CHECK(render_show(&r2, (const uint8_t*)reply2, strlen(reply2), out, sizeof(out)));
    CHECK(strcmp(out, "ON#") == 0);
}

static void test_show_text_rule_with_map(void) {
    ShowRule r = text_rule("PWR=", "\r");
    r.map_count = 2;
    strcpy(r.map[0].raw, "00");
    strcpy(r.map[0].word, "Standby");
    strcpy(r.map[1].raw, "01");
    strcpy(r.map[1].word, "On");
    char out[32];
    const char* reply = "PWR=00\r:";
    CHECK(render_show(&r, (const uint8_t*)reply, strlen(reply), out, sizeof(out)));
    CHECK(strcmp(out, "Standby") == 0);
}

static void test_show_binary_rule_with_map(void) {
    ShowRule r;
    memset(&r, 0, sizeof(r));
    r.binary = true;
    r.byte = 3;
    r.length = 1;
    r.map_count = 1;
    strcpy(r.map[0].raw, "01");
    strcpy(r.map[0].word, "On");
    const uint8_t reply[] = {0x70, 0x00, 0x02, 0x01, 0x01};
    char out[32];
    CHECK(render_show(&r, reply, sizeof(reply), out, sizeof(out)));
    CHECK(strcmp(out, "On") == 0);
    r.map_count = 0;
    r.byte = 2;
    r.length = 2;
    CHECK(render_show(&r, reply, sizeof(reply), out, sizeof(out)));
    CHECK(strcmp(out, "513") == 0);
    r.little = true;
    CHECK(render_show(&r, reply, sizeof(reply), out, sizeof(out)));
    CHECK(strcmp(out, "258") == 0);
}

static void test_show_missing_marker_returns_false(void) {
    ShowRule r = text_rule("LAMP=", "\r");
    char out[32];
    CHECK(!render_show(&r, (const uint8_t*)"ERR\r", 4, out, sizeof(out)));
    ShowRule b;
    memset(&b, 0, sizeof(b));
    b.binary = true;
    b.byte = 6;
    b.length = 1;
    const uint8_t short_reply[] = {1, 2, 3};
    CHECK(!render_show(&b, short_reply, sizeof(short_reply), out, sizeof(out)));
}

static void test_output_buffer_too_small_errors(void) {
    with_id(1);
    uint8_t out[4];
    CHECK(render_bytes("01 02 03 04 05", DataHex, &set, out, sizeof(out), err, sizeof(err)) == -1);
    CHECK(render_bytes("abcde", DataText, &set, out, sizeof(out), err, sizeof(err)) == -1);
}

int main(void) {
    RUN(test_samsung_mdc_power_on);
    RUN(test_sony_style_checksum);
    RUN(test_xor_checksum);
    RUN(test_hex_case_and_spacing);
    RUN(test_text_with_hex_id);
    RUN(test_escapes);
    RUN(test_unknown_placeholder_errors);
    RUN(test_value_out_of_byte_range_errors);
    RUN(test_bad_hex_errors);
    RUN(test_checksum_in_text_errors);
    RUN(test_pattern_wildcards_and_match);
    RUN(test_text_pattern);
    RUN(test_show_text_rule);
    RUN(test_show_text_rule_with_map);
    RUN(test_show_binary_rule_with_map);
    RUN(test_show_missing_marker_returns_false);
    RUN(test_output_buffer_too_small_errors);
    DONE("test_render");
}
