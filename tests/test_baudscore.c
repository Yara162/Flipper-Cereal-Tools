#include "check.h"
#include "baudscore.h"
#include "serialbits.h"

static void test_too_few_bytes_scores_zero(void) {
    BaudSample s = {.bytes = 3, .printable = 3};
    CHECK(baud_score(&s) == 0);
}

static void test_clean_text_scores_high(void) {
    BaudSample s = {.bytes = 100, .printable = 95};
    CHECK(baud_score(&s) >= 95);
}

static void test_clean_binary_still_likely(void) {
    BaudSample s = {.bytes = 100, .printable = 10};
    CHECK(baud_score(&s) >= 70);
}

static void test_frame_errors_lower_score(void) {
    BaudSample s = {.bytes = 100, .printable = 0, .frame_errors = 10};
    BaudSample t = {.bytes = 100, .printable = 0, .frame_errors = 0};
    CHECK(baud_score(&s) < baud_score(&t));
    BaudSample u = {.bytes = 100, .printable = 95, .frame_errors = 10};
    CHECK(baud_score(&u) > baud_score(&s)); // readable text counts in its favour
}

static void test_all_errors_scores_zero(void) {
    BaudSample s = {.bytes = 50, .frame_errors = 30, .parity_errors = 10, .noise_errors = 10};
    CHECK(baud_score(&s) == 0);
}

static void test_parity_stream_read_as_8n1_scores_lower(void) {
    // A 7-E-1 device heard at 8-N-1: no framing errors, but about half the bytes have bit 7 set
    BaudSample wrong = {.bytes = 100, .printable = 50};
    // The same device heard at 7-E-1, with the parity bit masked off
    BaudSample right = {.bytes = 100, .printable = 97};
    CHECK(baud_score(&right) > baud_score(&wrong));
    CHECK(baud_score(&wrong) >= 40); // still plausible, just not the best
}

static void test_candidate_order(void) {
    CHECK(baud_candidate_count == 16);
    CHECK(baud_candidates[0].baud == 1200 && baud_candidates[0].data_bits == 8 && baud_candidates[0].parity == 'N');
    CHECK(baud_candidates[7].baud == 115200);
    CHECK(baud_candidates[8].baud == 9600 && baud_candidates[8].data_bits == 7 && baud_candidates[8].parity == 'E');
    CHECK(baud_candidates[15].baud == 115200 && baud_candidates[15].data_bits == 8 && baud_candidates[15].parity == 'E');
}

static void test_data_mask(void) {
    SerialConfig seven = {9600, 7, 'E', 1}, eight = {9600, 8, 'N', 1};
    CHECK(serial_data_mask(&seven) == 0x7F);
    CHECK(serial_data_mask(&eight) == 0xFF);
    CHECK((0xC1 & serial_data_mask(&seven)) == 'A');
}

static const BaudProbe* probe(const char* name) {
    for(size_t i = 0; i < baud_probe_count; i++)
        if(strcmp(baud_probes[i].name, name) == 0) return &baud_probes[i];
    printf("no probe named %s\n", name);
    check_failures++;
    return &baud_probes[0];
}

static void test_probe_list(void) {
    CHECK(strcmp(baud_probes[0].name, "Off") == 0 && baud_probes[0].len == 0);
    const BaudProbe* p = probe("BenQ status");
    CHECK_EQ_BYTES(p->data, p->len, "\r*pow=?#\r", 9);
    p = probe("Carriage return");
    CHECK_EQ_BYTES(p->data, p->len, "\r", 1);
    p = probe("Epson status");
    CHECK_EQ_BYTES(p->data, p->len, "PWR?\r", 5);
    p = probe("LG status");
    CHECK_EQ_BYTES(p->data, p->len, "ka 01 FF\r", 9);
    const uint8_t mdc[] = {0xAA, 0x11, 0x01, 0x00, 0x12};
    p = probe("Samsung MDC status");
    CHECK_EQ_BYTES(p->data, p->len, mdc, sizeof(mdc));
    const uint8_t sony[] = {0x83, 0x00, 0x00, 0xFF, 0xFF, 0x81};
    p = probe("Sony status");
    CHECK_EQ_BYTES(p->data, p->len, sony, sizeof(sony));
    const uint8_t visca[] = {0x81, 0x09, 0x04, 0x00, 0xFF};
    p = probe("VISCA power");
    CHECK_EQ_BYTES(p->data, p->len, visca, sizeof(visca));
    for(size_t i = 0; i < baud_probe_count; i++) CHECK(strlen(baud_probes[i].name) <= 18);
}

static void test_loopback_verdicts(void) {
    size_t matched = 99;
    CHECK(loopback_judge(NULL, 0, &matched) == LoopbackNothing && matched == 0);
    CHECK(loopback_judge(loopback_pattern, loopback_pattern_len, &matched) == LoopbackPass);
    CHECK(matched == loopback_pattern_len);
    // stray bytes before the pattern (line noise at power-up) still pass
    uint8_t noisy[64] = {0x00, 0xFF};
    memcpy(noisy + 2, loopback_pattern, loopback_pattern_len);
    CHECK(loopback_judge(noisy, loopback_pattern_len + 2, &matched) == LoopbackPass);
    // only the first half came back
    CHECK(loopback_judge(loopback_pattern, loopback_pattern_len / 2, &matched) == LoopbackPartial);
    CHECK(matched == loopback_pattern_len / 2);
    // bytes arrived but none of them match: wrong framing, or something else is talking
    uint8_t junk[20];
    memset(junk, 0xF0, sizeof(junk));
    CHECK(loopback_judge(junk, sizeof(junk), &matched) == LoopbackGarbled && matched == 0);
}

int main(void) {
    RUN(test_probe_list);
    RUN(test_loopback_verdicts);
    RUN(test_data_mask);
    RUN(test_too_few_bytes_scores_zero);
    RUN(test_clean_text_scores_high);
    RUN(test_clean_binary_still_likely);
    RUN(test_frame_errors_lower_score);
    RUN(test_all_errors_scores_zero);
    RUN(test_parity_stream_read_as_8n1_scores_lower);
    RUN(test_candidate_order);
    DONE("test_baudscore");
}
