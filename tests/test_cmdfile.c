#include "check.h"
#include "cmdfile.h"
#include <stdlib.h>

static CommandSet set;
static CmdFileError err;

static bool parse(const char* json) {
    memset(&err, 0, sizeof(err));
    return cmdfile_parse(json, strlen(json), &set, &err);
}

static const char* full_file =
    "{\n"
    "  \"format\": 1,\n"
    "  \"id\": \"samsung_mdc\",\n"
    "  \"name\": \"Samsung MDC\",\n"
    "  \"category\": \"display\",\n"
    "  \"segment\": \"commercial\",\n"
    "  \"source\": \"Samsung MDC protocol guide v15.0\",\n"
    "  \"tested\": false,\n"
    "  \"serial\": { \"baud\": 9600, \"data_bits\": 8, \"parity\": \"none\", \"stop_bits\": 1 },\n"
    "  \"settings\": [\n"
    "    { \"key\": \"id\", \"label\": \"Display ID\", \"min\": 0, \"max\": 254, \"default\": 1 }\n"
    "  ],\n"
    "  \"commands\": [\n"
    "    { \"name\": \"Power on\", \"group\": \"Power\", \"hex\": \"AA 11 {id} 01 01 {sum1}\",\n"
    "      \"expect\": \"AA FF {id} 03 41 11 01\" }\n"
    "  ]\n"
    "}\n";

static void test_parses_full_file(void) {
    CHECK(parse(full_file));
    CHECK(set.command_count == 1);
    CHECK(set.setting_count == 1);
    CHECK(set.settings[0].value == 1);
    CHECK(set.settings[0].max == 254);
    CHECK(strcmp(set.settings[0].key, "id") == 0);
    CHECK(set.has_serial);
    CHECK(set.serial.baud == 9600);
    CHECK(set.serial.data_bits == 8);
    CHECK(set.serial.parity == 'N');
    CHECK(set.serial.stop_bits == 1);
    CHECK(set.tested == false);
    CHECK(strcmp(set.id, "samsung_mdc") == 0);
    CHECK(strcmp(set.category, "display") == 0);
    CHECK(strcmp(set.segment, "commercial") == 0);
    CHECK(set.commands[0].kind == DataHex);
    CHECK(strcmp(set.commands[0].name, "Power on") == 0);
    CHECK(strcmp(set.commands[0].group, "Power") == 0);
    CHECK(set.commands[0].has_expect);
    CHECK(set.commands[0].expect_kind == DataHex);
    CHECK(strcmp(set.commands[0].expect, "AA FF {id} 03 41 11 01") == 0);
}

static void test_text_command_and_show_rules(void) {
    const char* json =
        "{ \"format\": 1, \"id\": \"epson\", \"name\": \"Epson\", \"category\": \"projector\",\n"
        "  \"commands\": [\n"
        "    { \"name\": \"Power on\", \"text\": \"PWR ON\\r\", \"expect_text\": \":\" },\n"
        "    { \"name\": \"Lamp hours\", \"text\": \"LAMP?\\r\",\n"
        "      \"show\": [ { \"label\": \"Lamp hours\", \"after\": \"LAMP=\", \"until\": \"\\r\" } ] },\n"
        "    { \"name\": \"Status\", \"hex\": \"01 02\",\n"
        "      \"show\": [ { \"label\": \"Power\", \"byte\": 6, \"length\": 1,\n"
        "                  \"map\": { \"00\": \"Standby\", \"01\": \"On\" } } ] }\n"
        "  ] }\n";
    CHECK(parse(json));
    CHECK(set.command_count == 3);
    CHECK(set.commands[0].kind == DataText);
    CHECK(strcmp(set.commands[0].data, "PWR ON\r") == 0);
    CHECK(set.commands[0].has_expect && set.commands[0].expect_kind == DataText);
    CHECK(strcmp(set.commands[0].expect, ":") == 0);
    CHECK(set.show_count == 2);
    CHECK(set.shows[0].command == 1);
    CHECK(!set.shows[0].binary);
    CHECK(strcmp(set.shows[0].label, "Lamp hours") == 0);
    CHECK(strcmp(set.shows[0].after, "LAMP=") == 0);
    CHECK(strcmp(set.shows[0].until, "\r") == 0);
    CHECK(set.shows[1].command == 2);
    CHECK(set.shows[1].binary);
    CHECK(set.shows[1].byte == 6 && set.shows[1].length == 1 && !set.shows[1].little);
    CHECK(set.shows[1].map_count == 2);
    CHECK(strcmp(set.shows[1].map[1].raw, "01") == 0 && strcmp(set.shows[1].map[1].word, "On") == 0);
}

static void test_missing_comma_reports_line(void) {
    const char* json = "{\n"
                       "  \"format\": 1,\n"
                       "  \"id\": \"x\"\n"
                       "  \"name\": \"X\",\n"
                       "  \"commands\": []\n"
                       "}\n";
    CHECK(!parse(json));
    CHECK(err.line == 4);
    CHECK(err.message[0] != 0);
}

static void test_rejects_both_hex_and_text(void) {
    CHECK(!parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\",\n"
                 "  \"commands\": [ { \"name\": \"A\", \"hex\": \"01\", \"text\": \"a\" } ] }"));
    CHECK(strstr(err.message, "hex") != NULL);
    CHECK(err.line == 2);
}

static void test_rejects_wrong_format_version(void) {
    CHECK(!parse("{ \"format\": 2, \"id\": \"x\", \"name\": \"X\", \"commands\": [] }"));
    CHECK(strstr(err.message, "format") != NULL);
}

static void test_rejects_missing_format(void) {
    CHECK(!parse("{ \"id\": \"x\", \"name\": \"X\", \"commands\": [] }"));
    CHECK(strstr(err.message, "format") != NULL);
}

static void test_rejects_unknown_parity(void) {
    CHECK(!parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\",\n"
                 "  \"serial\": { \"baud\": 9600, \"parity\": \"mark\" }, \"commands\": [] }"));
    CHECK(strstr(err.message, "parity") != NULL);
}

static void test_rejects_unknown_command_field(void) {
    CHECK(!parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\",\n"
                 "  \"commands\": [ { \"name\": \"A\", \"hexx\": \"01\" } ] }"));
    CHECK(strstr(err.message, "hexx") != NULL);
}

static void test_rejects_command_without_data(void) {
    CHECK(!parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\", \"commands\": [ { \"name\": \"A\" } ] }"));
}

static void test_too_many_commands(void) {
    char* json = malloc(8192);
    int n = sprintf(json, "{ \"format\": 1, \"id\": \"x\", \"name\": \"X\", \"commands\": [");
    for(int i = 0; i < 49; i++) n += sprintf(json + n, "%s{ \"name\": \"C%d\", \"hex\": \"01\" }", i ? "," : "", i);
    sprintf(json + n, "] }");
    CHECK(!parse(json));
    CHECK(strstr(err.message, "48") != NULL);
    free(json);
}

static void test_optional_fields_absent(void) {
    CHECK(parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\", \"commands\": [ { \"name\": \"A\", \"hex\": \"01\" } ] }"));
    CHECK(!set.has_serial);
    CHECK(set.setting_count == 0);
    CHECK(set.category[0] == 0);
    CHECK(set.command_count == 1);
    CHECK(set.commands[0].group[0] == 0);
    CHECK(!set.commands[0].has_expect);
}

static void test_unknown_top_level_key_ignored(void) {
    CHECK(parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"X\", \"notes\": { \"a\": [1, 2] },\n"
                "  \"commands\": [ { \"name\": \"A\", \"hex\": \"01\" } ] }"));
}

static void test_json_escapes_decoded(void) {
    CHECK(parse("{ \"format\": 1, \"id\": \"x\", \"name\": \"Say \\\"hi\\\" \\u0041\",\n"
                "  \"commands\": [ { \"name\": \"A\", \"text\": \"a\\\\xFF\\r\\n\" } ] }"));
    CHECK(strcmp(set.name, "Say \"hi\" A") == 0);
    // JSON \\ becomes one backslash, leaving the command escape \xFF for the renderer
    CHECK(strcmp(set.commands[0].data, "a\\xFF\r\n") == 0);
}

static void test_data_too_long_errors(void) {
    char json[600];
    char data[200];
    memset(data, 'A', 180);
    data[180] = 0;
    sprintf(json, "{ \"format\": 1, \"id\": \"x\", \"name\": \"X\", \"commands\": [ { \"name\": \"A\", \"text\": \"%s\" } ] }", data);
    CHECK(!parse(json));
    CHECK(strstr(err.message, "long") != NULL);
}

static void test_not_json_errors(void) {
    CHECK(!parse("this is not json"));
    CHECK(!parse(""));
    CHECK(!parse("[1, 2, 3]"));
}

static CatalogEntry entry(const char* id, const char* name, bool user, bool broken) {
    CatalogEntry e;
    memset(&e, 0, sizeof(e));
    strcpy(e.id, id);
    strcpy(e.name, name);
    e.user = user;
    e.broken = broken;
    return e;
}

static void test_catalog_merge_user_wins(void) {
    CatalogEntry builtin[] = {entry("samsung_mdc", "Samsung MDC", false, false), entry("lg_ascii", "LG", false, false)};
    CatalogEntry user[] = {entry("samsung_mdc", "Samsung MDC (mine)", true, false)};
    CatalogEntry out[8];
    size_t n = catalog_merge(builtin, 2, user, 1, out, 8);
    CHECK(n == 2);
    CHECK(strcmp(out[0].id, "lg_ascii") == 0); // sorted by name
    CHECK(strcmp(out[1].id, "samsung_mdc") == 0 && out[1].user);
}

static void test_catalog_merge_keeps_broken(void) {
    CatalogEntry user[] = {entry("", "bad_file.json", true, true), entry("mine", "Mine", true, false)};
    CatalogEntry out[8];
    size_t n = catalog_merge(NULL, 0, user, 2, out, 8);
    CHECK(n == 2);
    CHECK(out[0].broken);
}

static void test_catalog_merge_respects_max(void) {
    CatalogEntry builtin[] = {entry("a", "A", false, false), entry("b", "B", false, false), entry("c", "C", false, false)};
    CatalogEntry out[2];
    CHECK(catalog_merge(builtin, 3, NULL, 0, out, 2) == 2);
}

int main(void) {
    RUN(test_catalog_merge_user_wins);
    RUN(test_catalog_merge_keeps_broken);
    RUN(test_catalog_merge_respects_max);
    RUN(test_parses_full_file);
    RUN(test_text_command_and_show_rules);
    RUN(test_missing_comma_reports_line);
    RUN(test_rejects_both_hex_and_text);
    RUN(test_rejects_wrong_format_version);
    RUN(test_rejects_missing_format);
    RUN(test_rejects_unknown_parity);
    RUN(test_rejects_unknown_command_field);
    RUN(test_rejects_command_without_data);
    RUN(test_too_many_commands);
    RUN(test_optional_fields_absent);
    RUN(test_unknown_top_level_key_ignored);
    RUN(test_json_escapes_decoded);
    RUN(test_data_too_long_errors);
    RUN(test_not_json_errors);
    DONE("test_cmdfile");
}
