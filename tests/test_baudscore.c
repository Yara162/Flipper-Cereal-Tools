#include "check.h"
#include "baudscore.h"

static void test_too_few_bytes_scores_zero(void) {
    BaudSample s = {.bytes = 3, .printable = 3};
    CHECK(baud_score(&s) == 0);
}

static void test_clean_text_scores_high(void) {
    BaudSample s = {.bytes = 100, .printable = 95};
    CHECK(baud_score(&s) == 100);
}

static void test_clean_binary_scores_100_without_bonus(void) {
    BaudSample s = {.bytes = 100, .printable = 10};
    CHECK(baud_score(&s) == 100);
}

static void test_frame_errors_lower_score(void) {
    BaudSample s = {.bytes = 100, .printable = 0, .frame_errors = 10};
    CHECK(baud_score(&s) == 60);
    BaudSample t = {.bytes = 100, .printable = 95, .frame_errors = 10};
    CHECK(baud_score(&t) == 70); // printable bonus
}

static void test_all_errors_scores_zero(void) {
    BaudSample s = {.bytes = 50, .frame_errors = 30, .parity_errors = 10, .noise_errors = 10};
    CHECK(baud_score(&s) == 0);
}

static void test_candidate_order(void) {
    CHECK(baud_candidate_count == 16);
    CHECK(baud_candidates[0].baud == 1200 && baud_candidates[0].data_bits == 8 && baud_candidates[0].parity == 'N');
    CHECK(baud_candidates[7].baud == 115200);
    CHECK(baud_candidates[8].baud == 9600 && baud_candidates[8].data_bits == 7 && baud_candidates[8].parity == 'E');
    CHECK(baud_candidates[15].baud == 115200 && baud_candidates[15].data_bits == 8 && baud_candidates[15].parity == 'E');
}

int main(void) {
    RUN(test_too_few_bytes_scores_zero);
    RUN(test_clean_text_scores_high);
    RUN(test_clean_binary_scores_100_without_bonus);
    RUN(test_frame_errors_lower_score);
    RUN(test_all_errors_scores_zero);
    RUN(test_candidate_order);
    DONE("test_baudscore");
}
