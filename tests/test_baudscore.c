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

int main(void) {
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
