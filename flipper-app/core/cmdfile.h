// Command files: the JSON format shared by built-in command sets and the owner's own files.
// No Flipper dependencies, so it can be tested on a PC.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t baud;
    uint8_t data_bits;
    char parity; // 'N', 'E' or 'O'
    uint8_t stop_bits;
} SerialConfig;

typedef enum { DataHex, DataText } DataKind;

typedef struct {
    char key[12];
    char label[20];
    int32_t min, max, def, value;
} CmdSetting;

typedef struct {
    char name[32];
    char group[16];
    DataKind kind;
    char data[160];
    bool has_expect;
    DataKind expect_kind;
    char expect[96];
} Command;

typedef struct {
    char raw[12];
    char word[20];
} ShowMap;

typedef struct {
    uint8_t command; // index of the command whose reply this reads
    char label[20];
    bool binary;
    char after[24]; // text replies
    char until[8];
    int16_t byte; // binary replies
    uint8_t length;
    bool little;
    ShowMap map[6];
    uint8_t map_count;
} ShowRule;

#define CMDSET_MAX_COMMANDS 48
#define CMDSET_MAX_SETTINGS 4
#define CMDSET_MAX_SHOWS 16

typedef struct {
    char id[32];
    char name[32];
    char category[12];
    char segment[12];
    char source[64];
    bool tested;
    bool has_serial;
    SerialConfig serial;
    CmdSetting settings[CMDSET_MAX_SETTINGS];
    uint8_t setting_count;
    Command commands[CMDSET_MAX_COMMANDS];
    uint8_t command_count;
    ShowRule shows[CMDSET_MAX_SHOWS];
    uint8_t show_count;
} CommandSet;

typedef struct {
    int line;
    char message[48];
} CmdFileError;

// Parses one command file. On failure returns false and fills `err` with the line and reason.
bool cmdfile_parse(const char* json, size_t len, CommandSet* out, CmdFileError* err);
