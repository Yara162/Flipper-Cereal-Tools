#include "check.h"
#include "msgsplit.h"

static MsgSplitter s;
static Message m;

static int feed_str(const char* str, uint32_t t) {
    int got = 0;
    for(const char* p = str; *p; p++)
        if(msgsplit_feed(&s, (uint8_t)*p, t, &m)) got++;
    return got;
}

static void test_crlf_is_one_message(void) {
    msgsplit_init(&s, 50);
    int got = feed_str("abc\r", 1000);
    got += feed_str("\n", 1001);
    CHECK(got == 1);
    CHECK_EQ_BYTES(m.data, m.len, "abc\r\n", 5);
    CHECK(m.at_ms == 1000);
    CHECK(!msgsplit_poll(&s, 2000, &m)); // nothing left over
}

static void test_lf_ends_message(void) {
    msgsplit_init(&s, 50);
    CHECK(feed_str("ok\n", 10) == 1);
    CHECK_EQ_BYTES(m.data, m.len, "ok\n", 3);
}

static void test_lone_cr_ends_after_gap(void) {
    msgsplit_init(&s, 50);
    CHECK(feed_str("abc\r", 100) == 0);
    CHECK(!msgsplit_poll(&s, 120, &m));
    CHECK(msgsplit_poll(&s, 151, &m));
    CHECK_EQ_BYTES(m.data, m.len, "abc\r", 4);
}

static void test_cr_then_text_splits(void) {
    msgsplit_init(&s, 50);
    CHECK(feed_str("abc\r", 100) == 0);
    CHECK(feed_str("x", 101) == 1); // the \r message is released when another byte arrives
    CHECK_EQ_BYTES(m.data, m.len, "abc\r", 4);
    CHECK(msgsplit_poll(&s, 200, &m));
    CHECK_EQ_BYTES(m.data, m.len, "x", 1);
    CHECK(m.at_ms == 101);
}

static void test_gap_splits_binary(void) {
    msgsplit_init(&s, msgsplit_gap_for_baud(9600));
    CHECK(!msgsplit_feed(&s, 0xAA, 0, &m));
    CHECK(!msgsplit_feed(&s, 0x11, 1, &m));
    CHECK(msgsplit_feed(&s, 0xBB, 61, &m)); // the gap ended the first message
    const uint8_t first[] = {0xAA, 0x11};
    CHECK_EQ_BYTES(m.data, m.len, first, 2);
    CHECK(msgsplit_poll(&s, 200, &m));
    CHECK(m.len == 1 && m.data[0] == 0xBB);
}

static void test_long_stream_truncates(void) {
    msgsplit_init(&s, 50);
    int got = 0;
    Message first;
    for(int i = 0; i < 300; i++)
        if(msgsplit_feed(&s, (uint8_t)('a' + i % 26), 0, &m)) {
            got++;
            first = m;
        }
    CHECK(got == 1);
    CHECK(first.len == 256 && first.truncated);
    CHECK(msgsplit_poll(&s, 100, &m));
    CHECK(m.len == 44 && !m.truncated);
}

static void test_line_end_after_gap_stays_separate(void) {
    msgsplit_init(&s, 50);
    CHECK(feed_str("abc", 0) == 0);
    CHECK(msgsplit_feed(&s, '\n', 100, &m)); // gap releases "abc"; "\n" completes on its own
    CHECK_EQ_BYTES(m.data, m.len, "abc", 3);
    CHECK(msgsplit_feed(&s, 'x', 101, &m)); // the finished "\n" comes out before x is added
    CHECK_EQ_BYTES(m.data, m.len, "\n", 1);
    CHECK(msgsplit_poll(&s, 200, &m));
    CHECK_EQ_BYTES(m.data, m.len, "x", 1);
}

static void test_poll_releases_completed_message(void) {
    msgsplit_init(&s, 50);
    CHECK(feed_str("abc", 0) == 0);
    CHECK(msgsplit_feed(&s, '\n', 100, &m));
    CHECK(msgsplit_poll(&s, 101, &m)); // completed, so no need to wait for the gap
    CHECK_EQ_BYTES(m.data, m.len, "\n", 1);
}

static void test_poll_without_data(void) {
    msgsplit_init(&s, 50);
    CHECK(!msgsplit_poll(&s, 1000, &m));
}

static void test_gap_for_baud(void) {
    CHECK(msgsplit_gap_for_baud(9600) == 50);
    CHECK(msgsplit_gap_for_baud(115200) == 20);
    CHECK(msgsplit_gap_for_baud(1200) == 400);
    CHECK(msgsplit_gap_for_baud(0) == 400);
}

int main(void) {
    RUN(test_crlf_is_one_message);
    RUN(test_lf_ends_message);
    RUN(test_lone_cr_ends_after_gap);
    RUN(test_cr_then_text_splits);
    RUN(test_gap_splits_binary);
    RUN(test_long_stream_truncates);
    RUN(test_line_end_after_gap_stays_separate);
    RUN(test_poll_releases_completed_message);
    RUN(test_poll_without_data);
    RUN(test_gap_for_baud);
    DONE("test_msgsplit");
}
