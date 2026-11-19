#include "check.h"
#include "reply.h"

static void test_waits_for_quiet_after_match(void) {
    // matched at 20 ms with more bytes due: not done until 150 ms of quiet
    CHECK(!reply_complete(true, 4, 0, 20, 20));
    CHECK(!reply_complete(true, 8, 0, 100, 60));
    CHECK(reply_complete(true, 8, 0, 211, 60));
}

static void test_quiet_after_data_ends_it(void) {
    CHECK(reply_complete(false, 5, 0, 300, 100));
    CHECK(!reply_complete(false, 5, 0, 200, 100));
}

static void test_timeout_ends_it(void) {
    CHECK(!reply_complete(false, 0, 0, 999, 0));
    CHECK(reply_complete(false, 0, 0, 1000, 0));
}

static void test_full_buffer_ends_it(void) {
    CHECK(reply_complete(false, REPLY_MAX, 0, 10, 10));
}

int main(void) {
    RUN(test_waits_for_quiet_after_match);
    RUN(test_quiet_after_data_ends_it);
    RUN(test_timeout_ends_it);
    RUN(test_full_buffer_ends_it);
    DONE("test_reply");
}
