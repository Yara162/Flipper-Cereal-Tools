# Cereal Tools Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first release of Cereal Tools, a Flipper Zero app with seven RS-232 tools for AV commissioning on the Flip'n Cereal HAT, plus the first batch of command sets.

**Architecture:** One external Flipper app (`appid` `cereal_tools`) built with ufbt. Protocol logic lives in plain C modules with no Flipper dependencies, tested on a PC with gcc and AddressSanitizer: JSON command files, byte rendering, message splitting, text/hex formatting, VISCA, baud scoring. Flipper-specific modules wrap the serial port, storage and screens. Built-in command sets ship as JSON assets; the owner's files live in the app's data folder.

**Tech Stack:** C (gnu17), Flipper SDK for firmware 1.5.1 (API 88.2) via ufbt; jsmn (MIT) for JSON; gcc + `-fsanitize=address,undefined` for host tests; GNU make.

**Spec:** `docs/superpowers/specs/2026-11-19-cereal-tools-design.md`

## Global Constraints

- Target: official Flipper firmware 1.5.1, API 88.2; build with `ufbt` (SDK at `$UFBT_HOME`).
- Serial port: USART (`FuriHalSerialIdUsart`, pins 13 TX / 14 RX). Disable the expansion service while the app runs (`expansion_disable`), restore on exit (`expansion_enable`). Keep the port id in one constant so a switch to LPUART is a one-line change.
- Nothing is transmitted except as the direct result of a button press, except: VISCA server replies, Echo auto-echo when the owner turned it on, and the baud finder's active probe when the owner turned it on for that run.
- Built-in command sets: `flipper-app/assets/displays/*.json` and `flipper-app/assets/projectors/*.json`, shipped via `fap_file_assets="assets"` (they land in `SD Card/apps_assets/cereal_tools/`).
- Owner files: `SD Card/apps_data/cereal_tools/commands/*.json`. Logs: `SD Card/apps_data/cereal_tools/logs/<tool>_<YYYYMMDD>_<HHMMSS>.txt`. Settings: `SD Card/apps_data/cereal_tools/settings.txt`.
- JSON `format` value is `1`. Every built-in file has `"tested": false` until the owner reports it working.
- Baud rates offered: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200. Data bits 7 or 8; parity none/even/odd; stop bits 1 or 2.
- VISCA defaults: 9600 baud, 8 data bits, no parity, 1 stop bit.
- Screen 128 x 64; small font lines hold about 25 characters. Follow the screen style of `/home/user/flipper-wifi-tools/flipper-app/wifi_tools.c` (title line with rule at y=10, rows of 9 px, hint on the bottom line at y=63).
- Licence GPL-3.0, copyright Yara162; keep jsmn's MIT notice in `flipper-app/lib/jsmn/LICENSE`.
- Commits authored `Yara162 <Yara162@users.noreply.github.com>`, messages ending with the session attribution lines.
- User-facing text (screens, README) in plain English, sentence case, no jargon the owner would not use.

## Review Focus

1. **A JSON file with an error** (missing comma, wrong type, unknown command field, both `hex` and `text`): the owner sees the file name, the line number and the reason; the file is skipped, the app keeps running. Tested in Task 1.
2. **Bytes that are not text** (0x00, 0xFF, high bytes, a stray `\r` mid-line) arriving at Echo or the sniffer: shown as `\xNN` or hex, never breaking the screen or the log. Tested in Task 3.
3. **A flood of data** (a device streaming continuously at 115200): the receive buffer overflows gracefully (counted as dropped, shown), messages longer than the buffer are split and marked, the UI stays responsive. Tested in Task 3 (splitter) and checked in Task 5.
4. **A placeholder that does not resolve or a value out of range** (`{idd}` typo, display ID 300 into a one-byte field): the command is not sent and the reason is shown. Tested in Task 2.
5. **An owner file with the same `id` as a built-in set:** the owner's file is used, marked as theirs, and the built-in is hidden. Tested in Task 7.

---

## File Structure

```
flipper-cereal-tools/
  README.md                         overview, wiring, file format, status table, Forgejo note
  LICENSE                           GPL-3.0
  .gitignore
  bin/                              prebuilt cereal_tools.fap
  flipper-app/
    application.fam
    cereal_tools.c                  entry point, App struct setup, main menu, view switching
    app.h                           App struct shared by tools, view ids, tool entry points
    serial_port.c/.h                USART open/close/configure/write, timestamped receive queue, error counts
    settings.c/.h                   SerialConfig persistence + settings screen
    log.c/.h                        log files (header with date, tool, serial settings)
    catalog.c/.h                    scan built-in and owner folders, merge by id
    cmdset_view.c/.h                shared command-set screen: settings rows, grouped list, send, result
    tool_echo.c, tool_sniffer.c, tool_custom.c, tool_display.c, tool_projector.c,
    tool_visca.c, tool_baud.c       one file per tool
    core/                           no Flipper includes; host-tested
      cmdfile.c/.h                  JSON command file -> CommandSet
      render.c/.h                   command/expect rendering, placeholders, checksums, matching, value extraction
      msgsplit.c/.h                 split a byte stream into messages
      fmt.c/.h                      bytes -> text with escapes / hex
      visca.c/.h                    VISCA framing, client commands, reply decoding, camera emulation
      baudscore.c/.h                scoring of baud/framing candidates
    lib/jsmn/jsmn.h, LICENSE
    assets/displays/*.json, assets/projectors/*.json
    examples/my_device.json         example written to the owner's folder when empty
  tests/
    Makefile                        builds and runs every test_*.c with ASan/UBSan
    check.h                         CHECK(cond) / CHECK_EQ_BYTES(a, alen, b, blen) macros, failure count
    test_cmdfile.c, test_render.c, test_msgsplit.c, test_fmt.c,
    test_visca.c, test_baudscore.c, test_assets.c
```

Shared types (defined in Task 1 and Task 2, used everywhere):

```c
// core/cmdfile.h
typedef struct { uint32_t baud; uint8_t data_bits; char parity; /* 'N','E','O' */ uint8_t stop_bits; } SerialConfig;
typedef enum { DataHex, DataText } DataKind;
typedef struct { char key[12]; char label[20]; int32_t min, max, def, value; } CmdSetting;
typedef struct { char name[32]; char group[16]; DataKind kind; char data[160];
                 bool has_expect; DataKind expect_kind; char expect[96]; } Command;
typedef struct { char raw[12]; char word[20]; } ShowMap;
typedef struct { uint8_t command; char label[20]; bool binary;
                 char after[24]; char until[8];            // text replies
                 int16_t byte; uint8_t length; bool little; // binary replies
                 ShowMap map[6]; uint8_t map_count; } ShowRule;
#define CMDSET_MAX_COMMANDS 48
#define CMDSET_MAX_SETTINGS 4
#define CMDSET_MAX_SHOWS 16
typedef struct { char id[32]; char name[32]; char category[12]; char segment[12]; char source[64];
                 bool tested; bool has_serial; SerialConfig serial;
                 CmdSetting settings[CMDSET_MAX_SETTINGS]; uint8_t setting_count;
                 Command commands[CMDSET_MAX_COMMANDS]; uint8_t command_count;
                 ShowRule shows[CMDSET_MAX_SHOWS]; uint8_t show_count; } CommandSet;
typedef struct { int line; char message[48]; } CmdFileError;
```

---

### Task 1: Repository scaffold, host test harness and JSON command file parser

**Files:**
- Create: `.gitignore`, `LICENSE` (GPL-3.0 text), `flipper-app/lib/jsmn/jsmn.h`, `flipper-app/lib/jsmn/LICENSE` (from github.com/zserge/jsmn, MIT), `flipper-app/core/cmdfile.h`, `flipper-app/core/cmdfile.c`, `tests/Makefile`, `tests/check.h`
- Test: `tests/test_cmdfile.c`

**Interfaces:**
- Produces: the shared types above; `bool cmdfile_parse(const char* json, size_t len, CommandSet* out, CmdFileError* err);`

- [ ] **Step 1: Write the failing tests** in `tests/test_cmdfile.c`:
  - `test_parses_full_file`: the design's example file (Samsung MDC, one `hex` command with `expect`, one setting `id` 0..254 default 1, serial 9600 8 N 1) parses; `out.command_count == 1`, `out.setting_count == 1`, `out.settings[0].value == 1`, `out.serial.baud == 9600`, `out.serial.parity == 'N'`, `out.tested == false`, `out.commands[0].kind == DataHex`, `strcmp(out.commands[0].expect, "AA FF {id} 03 41 11 01") == 0`.
  - `test_text_command_and_show_rules`: a projector file with `"text": "LAMP?\\r"` and `"show": [{ "label": "Lamp hours", "after": "LAMP=", "until": "\r" }]` and a binary rule `{ "label": "Power", "byte": 6, "length": 1, "map": { "00": "Standby", "01": "On" } }` parses into `shows[]` with `command` pointing at the owning command and `map_count == 2`.
  - `test_missing_comma_reports_line`: a 6-line file with the comma missing at the end of line 3 returns `false`, `err.line == 4` (the line where the parser fails), `err.message` non-empty.
  - `test_rejects_both_hex_and_text`: returns `false`, message contains `"hex"`.
  - `test_rejects_wrong_format_version`: `"format": 2` returns `false`, message contains `"format"`.
  - `test_rejects_unknown_parity`: `"parity": "mark"` returns `false`.
  - `test_too_many_commands`: 49 commands returns `false`, message contains `"48"`.
  - `test_optional_fields_absent`: a file with only `format`, `id`, `name` and one command parses; `has_serial == false`, `setting_count == 0`, `category` empty.

- [ ] **Step 2: Run** `make -C tests` — expected: build fails (no `cmdfile.c`).

- [ ] **Step 3: Implement** `cmdfile_parse` in `flipper-app/core/cmdfile.c` with jsmn (`JSMN_PARENT_LINKS` on, `JSMN_STATIC`): tokenise into a heap token array sized by a first `jsmn_parse(NULL)` count pass, walk the object, copy strings with JSON escapes decoded (`\"`, `\\`, `\/`, `\n`, `\r`, `\t`, `\uXXXX` for ASCII), truncate overlong strings silently except `data`/`expect` (error if longer than the field). Line numbers come from counting `\n` up to the failing token's start (or jsmn's stop position on a syntax error). Unknown top-level keys are ignored (forward compatible); unknown keys inside a command are an error naming the key. `tests/Makefile` compiles each `tests/test_*.c` with `../flipper-app/core/*.c -I../flipper-app/core -I../flipper-app/lib/jsmn -std=gnu17 -Wall -Wextra -Werror -g -fsanitize=address,undefined` and runs it; `check.h` prints `file:line: CHECK failed: expr` and the `main` of each test returns non-zero when any check failed.

- [ ] **Step 4: Run** `make -C tests` — expected: `test_cmdfile: all passed`.

- [ ] **Step 5: Commit** — `git add .gitignore LICENSE flipper-app/lib flipper-app/core/cmdfile.* tests && git commit -m "Add JSON command file parser and host test harness"`

### Task 2: Rendering commands, matching replies and reading values

**Files:**
- Create: `flipper-app/core/render.h`, `flipper-app/core/render.c`
- Test: `tests/test_render.c`

**Interfaces:**
- Consumes: `CommandSet`, `Command`, `ShowRule`, `DataKind` (Task 1).
- Produces:
  - `int render_bytes(const char* data, DataKind kind, const CommandSet* set, uint8_t* out, size_t out_max, char* err, size_t err_size);` returns length or -1.
  - `int render_pattern(const char* data, DataKind kind, const CommandSet* set, uint8_t* bytes, uint8_t* mask, size_t max, char* err, size_t err_size);` like `render_bytes`, plus `??` in hex means "any byte" (`mask[i] = 0`).
  - `bool render_match(const uint8_t* reply, size_t reply_len, const uint8_t* bytes, const uint8_t* mask, size_t pattern_len);` true if the pattern occurs anywhere in the reply.
  - `bool render_show(const ShowRule* rule, const uint8_t* reply, size_t len, char* out, size_t out_size);` writes the value (mapped word if a map entry matches).

Placeholder rules (pin these in tests):
- `{key}` takes the setting's current value. In hex data it is one byte (error if outside 0..255). In text it is decimal.
- Text format suffixes: `{key:2d}` zero-padded decimal width 2, `{key:2x}` lower-case hex width 2, `{key:2X}` upper-case hex width 2 (widths 1 to 4).
- `{sum}` (hex only): low byte of the sum of every byte before it. `{sum1}` (hex only): the same but skipping the first byte. `{xor}` (hex only): XOR of every byte before it.
- Unknown placeholder: error naming it. Text escapes: `\r \n \t \\ \xNN`.

- [ ] **Step 1: Write the failing tests** in `tests/test_render.c`:
  - `test_samsung_mdc_power_on`: set with setting `id`=1; `"AA 11 {id} 01 01 {sum1}"` renders to `AA 11 01 01 01 14`.
  - `test_sony_style_checksum`: `"8C 00 00 02 01 {sum}"` renders to `8C 00 00 02 01 8F`.
  - `test_text_with_hex_id`: setting `id`=1; `"ka {id:2x} 01\r"` renders to the 9 bytes `"ka 01 01\r"`.
  - `test_escapes`: `"*pow=on#\r"` → `2A 70 6F 77 3D 6F 6E 23 0D`; `"\x02PON\x03"` → `02 50 4F 4E 03`.
  - `test_unknown_placeholder_errors`: `"{idd}"` returns -1, `err` contains `"idd"`.
  - `test_value_out_of_byte_range_errors`: `id`=300 in hex returns -1.
  - `test_pattern_wildcards_and_match`: pattern `"AA FF ?? 03 41"` matches reply `AA FF 05 03 41 11 01 5A`; does not match `AA FF 05 03 4E`.
  - `test_show_text_rule`: reply `"LAMP=01234\r:"`, rule after `"LAMP="` until `"\r"` → `"01234"`.
  - `test_show_binary_rule_with_map`: reply `70 00 02 01 01`, rule byte 3 length 1 map `{"01":"On"}` → `"On"`; rule byte 2 length 2 no map → `"513"` (big-endian `02 01`); same with `"order": "little"` → `"258"`.
  - `test_show_missing_marker_returns_false`.
  - `test_output_buffer_too_small_errors`.

- [ ] **Step 2: Run** `make -C tests` — expected: link failure for `render_*`.
- [ ] **Step 3: Implement** the four functions in `flipper-app/core/render.c`. Hex parsing accepts upper or lower case, spaces optional between pairs.
- [ ] **Step 4: Run** `make -C tests` — expected: all test programs pass.
- [ ] **Step 5: Commit** — `git commit -m "Add command rendering, reply matching and value reading"`

### Task 3: Message splitting and text/hex formatting

**Files:**
- Create: `flipper-app/core/msgsplit.h/.c`, `flipper-app/core/fmt.h/.c`
- Test: `tests/test_msgsplit.c`, `tests/test_fmt.c`

**Interfaces:**
- Produces:
  - `#define MSG_MAX 256`; `typedef struct { uint8_t data[MSG_MAX]; uint16_t len; uint32_t at_ms; bool truncated; } Message;`
  - `typedef struct MsgSplitter MsgSplitter;` (struct defined in header) with `void msgsplit_init(MsgSplitter* s, uint32_t gap_ms);`, `bool msgsplit_feed(MsgSplitter* s, uint8_t byte, uint32_t now_ms, Message* out);`, `bool msgsplit_poll(MsgSplitter* s, uint32_t now_ms, Message* out);`, `uint32_t msgsplit_gap_for_baud(uint32_t baud);`
  - `size_t fmt_text(const uint8_t* data, size_t len, char* out, size_t out_size);` printable ASCII as is, `\r`, `\n`, `\t`, `\\`, others `\xNN`; always NUL-terminated; stops cleanly at `out_size`.
  - `size_t fmt_hex(const uint8_t* data, size_t len, char* out, size_t out_size);` `"AA 11 01"`.
- Rules: a message ends at `\n`, or at `\r` not followed by `\n` within the gap, or after `gap_ms` of silence, or when it reaches `MSG_MAX` (then `truncated = true` and the next bytes start a new message). `\r\n` stays together in one message. `at_ms` is the time of the first byte. `msgsplit_gap_for_baud(b) = max(20, 480000 / b)` ms.

- [ ] **Step 1: Write the failing tests:** `test_crlf_is_one_message` (`"abc\r\n"` → one message of 5 bytes); `test_lone_cr_ends_after_gap` (`"abc\r"` then `msgsplit_poll` at +gap → one message); `test_gap_splits_binary` (`AA 11` then 60 ms silence then `BB` at 9600 → two messages); `test_long_stream_truncates` (300 bytes with no gaps → message of 256 with `truncated`, then a 44-byte one); `test_gap_for_baud` (9600 → 50, 115200 → 20, 1200 → 400); `test_fmt_text_escapes` (`00 41 0D 0A FF 5C` → `\x00A\r\n\xFF\\`); `test_fmt_small_buffer` (output never overruns, ends with NUL); `test_fmt_hex`.
- [ ] **Step 2: Run** `make -C tests` — expected: failures for the missing modules.
- [ ] **Step 3: Implement** both modules.
- [ ] **Step 4: Run** `make -C tests` — expected: all pass.
- [ ] **Step 5: Commit** — `git commit -m "Add message splitter and text/hex formatting"`

### Task 4: App shell, serial port, settings screen, logs

**Files:**
- Create: `flipper-app/application.fam`, `flipper-app/app.h`, `flipper-app/cereal_tools.c`, `flipper-app/serial_port.h/.c`, `flipper-app/settings.h/.c`, `flipper-app/log.h/.c`

**Interfaces:**
- Consumes: `SerialConfig` (Task 1).
- Produces:
  - `application.fam`: `appid="cereal_tools"`, `name="Cereal Tools"`, `apptype=FlipperAppType.EXTERNAL`, `entry_point="cereal_tools_app"`, `requires=["gui","storage"]`, `stack_size=4*1024`, `fap_category="GPIO"`, `fap_version="0.1"`, `fap_file_assets="assets"`, `fap_include_paths=["core", "lib/jsmn"]` (jsmn is header-only; the default source glob already compiles `core/*.c`).
  - `typedef struct { uint8_t byte; uint8_t errors; uint32_t at_ms; } SerialByte;` with error bits `SERIAL_ERR_FRAME 1`, `SERIAL_ERR_PARITY 2`, `SERIAL_ERR_NOISE 4`, `SERIAL_ERR_OVERRUN 8`.
  - `typedef struct { uint32_t frame, parity, noise, overrun, dropped; } SerialErrors;`
  - `SerialPort* serial_port_open(const SerialConfig* cfg);` (NULL if the USART is busy), `void serial_port_close(SerialPort* p);`, `void serial_port_configure(SerialPort* p, const SerialConfig* cfg);`, `void serial_port_write(SerialPort* p, const uint8_t* data, size_t len);`, `size_t serial_port_read(SerialPort* p, SerialByte* out, size_t max);`, `void serial_port_errors(SerialPort* p, SerialErrors* out, bool reset);`
  - `void settings_load(SerialConfig* cfg);`, `void settings_save(const SerialConfig* cfg);` (defaults 9600 8 N 1), `View* settings_view_alloc(App* app);`
  - `void serial_config_label(const SerialConfig* cfg, char* out, size_t size);` → `"9600 8-N-1"`.
  - `File* log_open(const char* tool, const SerialConfig* cfg, char* name_out, size_t name_size);`, `void log_line(File* f, const char* text);`, `void log_close(File* f);`
  - `app.h`: `typedef struct App { Gui* gui; ViewDispatcher* dispatcher; Submenu* menu; SerialPort* port; SerialConfig config; ... } App;`, view ids enum `AppViewMenu, AppViewSettings, AppViewEcho, AppViewSniffer, AppViewCustom, AppViewDisplay, AppViewProjector, AppViewVisca, AppViewBaud, AppViewCmdSet, AppViewTextInput`, and `void app_apply_config(App* app, const SerialConfig* cfg);` (configures port, saves settings).
- Receive path: the RX interrupt callback pushes `SerialByte` (timestamp from `furi_get_tick()` converted to ms; error flags from the event) into a `FuriStreamBuffer` sized for 2048 entries; when full, increment `dropped`.
- Main menu items, in order: Display control, Projector control, Custom commands, Echo, Sniffer, VISCA camera, Baud-rate finder, Serial settings. The menu header shows the current settings label. Tools not yet built show a "Coming in a later step" message until their task lands.

- [ ] **Step 1: Build** — `cd flipper-app && ufbt` — expected: `cereal_tools.fap` builds with no warnings (ufbt treats warnings as errors).
- [ ] **Step 2: Check on the host** that `make -C tests` still passes (core untouched).
- [ ] **Step 3: Commit** — `git commit -m "Add app shell, serial port, settings screen and logs"`

### Task 5: Echo tool

**Files:**
- Create: `flipper-app/tool_echo.c`; Modify: `flipper-app/cereal_tools.c` (menu hookup)

**Interfaces:**
- Consumes: `serial_port_*`, `msgsplit_*`, `fmt_*`, `log_*`, `App`.
- Produces: `View* echo_view_alloc(App* app); void echo_view_free(View* v);`
- Behaviour (from the spec): last messages listed newest at the bottom, scrollable (Up/Down); Left toggles text/hex; OK sends the most recent message back byte for byte (status line shows "Echoed 12 bytes"); hold OK toggles auto-echo (status line shows `AUTO`); Right saves the session to a log; counters of messages and bytes; keeps the last 32 messages. Dropped bytes and framing/parity errors shown on the status line when non-zero.

- [ ] **Step 1: Build** with `ufbt` — expected: no warnings.
- [ ] **Step 2: Commit** — `git commit -m "Add Echo tool"`

### Task 6: Sniffer tool

**Files:**
- Create: `flipper-app/tool_sniffer.c`; Modify: `flipper-app/cereal_tools.c`

**Interfaces:**
- Produces: `View* sniffer_view_alloc(App* app); void sniffer_view_free(View* v);`
- Behaviour: never writes to the port. Each line `mm:ss.mmm` (time since start) then text or hex (Left toggles). Up/Down scroll, keeps the last 64 messages on screen. Status line: message count, framing and parity error counts, dropped count. OK starts/stops logging; while logging every message is written as `mm:ss.mmm  <hex>  |  <text>`. Right opens serial settings (so the owner can change baud without leaving).

- [ ] **Step 1: Build** with `ufbt` — expected: no warnings.
- [ ] **Step 2: Commit** — `git commit -m "Add Sniffer tool"`

### Task 7: Catalog, shared command-set screen and Custom commands tool

**Files:**
- Create: `flipper-app/catalog.h/.c`, `flipper-app/cmdset_view.h/.c`, `flipper-app/tool_custom.c`, `flipper-app/examples/my_device.json`
- Modify: `flipper-app/cereal_tools.c`
- Test: `tests/test_cmdfile.c` (add the merge-rule test via a pure helper)

**Interfaces:**
- Consumes: `cmdfile_parse`, `render_*`, `fmt_*`, `serial_port_*`, `app_apply_config`, `log_*`.
- Produces:
  - In core (host-testable): `typedef struct { char path[96]; char id[32]; char name[32]; char segment[12]; char category[12]; bool tested; bool user; bool broken; int error_line; char error[48]; } CatalogEntry;` and `size_t catalog_merge(CatalogEntry* builtin, size_t nb, CatalogEntry* user, size_t nu, CatalogEntry* out, size_t max);` in `flipper-app/core/cmdfile.c` (declared in `cmdfile.h`): user entries replace built-in entries with the same `id`; output sorted by name; broken files kept (listed with a mark so the owner sees the error).
  - `size_t catalog_scan(const char* category, bool include_builtin, CatalogEntry* out, size_t max);` in `catalog.c`: `category` `"display"`, `"projector"` or `NULL` (owner files of any category).
  - `void cmdset_view_open(App* app, const char* path);` loads the file, applies its `serial` block via `app_apply_config` (status line "Settings from file: 9600 8-N-1"), shows settings rows first (Left/Right change values within min..max), then commands grouped under their `group` headers. OK sends (`render_bytes`); the result screen shows TX in hex, RX in text or hex (Left toggles), `OK` / `UNEXPECTED` if the command has an expect pattern (wait up to 1000 ms, end early when it matches), and every `show` value for that command. OK on the result screen sends again. Errors from rendering are shown instead of sending. Hold Right saves the session history (last 32 sends) to a log.
  - Custom commands tool: lists owner files (`catalog_scan(NULL, false, ...)`); broken files shown as `! name` and opening one shows `file`, `line`, `reason`. If the folder has no `.json` files, copy `examples/my_device.json` (bundled in assets as `assets/examples/my_device.json`) into it and say so.

- [ ] **Step 1: Write the failing test** `test_catalog_merge_user_wins` in `tests/test_cmdfile.c`: built-in `{samsung_mdc, lg_ascii}`, user `{samsung_mdc (user)}` → output has 2 entries, the `samsung_mdc` one has `user == true`; `test_catalog_merge_keeps_broken` keeps a broken user entry.
- [ ] **Step 2: Run** `make -C tests` — expected: failure (no `catalog_merge`).
- [ ] **Step 3: Implement** `catalog_merge`, then the Flipper modules.
- [ ] **Step 4: Run** `make -C tests` and `cd flipper-app && ufbt` — expected: tests pass, build clean.
- [ ] **Step 5: Commit** — `git commit -m "Add command-set screen, catalog and Custom commands tool"`

### Task 8: First batch of command sets

**Files:**
- Create: `flipper-app/assets/displays/samsung_mdc.json`, `sony_bravia.json`, `lg_ascii.json`, `flipper-app/assets/projectors/benq.json`, `epson_escvp21.json`, `flipper-app/assets/examples/my_device.json` (moved from `examples/`)
- Test: `tests/test_assets.c`

**Interfaces:**
- Consumes: `cmdfile_parse`, `render_bytes`, `render_pattern` (Tasks 1, 2).
- Requirements for every file: `format` 1, unique `id`, `category`, `segment` (`commercial`, `residential` or `both`; LG and Sony Bravia are `both`), `source` naming the manufacturer document used, `"tested": false`, a `serial` block, commands for Power on, Power off, the inputs the protocol supports (HDMI 1-4 at least where present), Volume up, Volume down, Mute (on/off or toggle as the protocol has it); projectors: Power on/off, Inputs, Picture mute on/off, and status queries with `show` rules for power state and lamp/light hours, plus error status where the protocol has it. Commands are researched from public manufacturer documents (web search); where a document cannot be found the command is left out and noted in the README, never guessed.
- Known values to pin in tests (from the protocols' public documents; if research shows one of these is wrong, fix the test and say so in the commit message):
  - Samsung MDC (ID 1): Power on `AA 11 01 01 01 14`.
  - Sony Bravia: Power on `8C 00 00 02 01 8F`, Power off `8C 00 00 02 00 8E`.
  - LG (set ID 1): Power off `"ka 01 00\r"`.
  - BenQ: Power on `"\r*pow=on#\r"`.
  - Epson ESC/VP21: Power on `"PWR ON\r"`, lamp query `"LAMP?\r"` with a show rule `after "LAMP="`.

- [ ] **Step 1: Write the failing test** `tests/test_assets.c`: for every file in `flipper-app/assets/**/*.json`, `cmdfile_parse` succeeds and every command and every expect pattern renders with default settings; `id`s are unique; `tested == false`; plus the pinned values above.
- [ ] **Step 2: Run** `make -C tests` — expected: failure (no assets yet).
- [ ] **Step 3: Research and write** the five files.
- [ ] **Step 4: Run** `make -C tests` — expected: all pass.
- [ ] **Step 5: Commit** — `git commit -m "Add first batch of display and projector command sets"`

### Task 9: Display control and Projector control tools

**Files:**
- Create: `flipper-app/tool_display.c`, `flipper-app/tool_projector.c`; Modify: `flipper-app/cereal_tools.c`

**Interfaces:**
- Consumes: `catalog_scan`, `cmdset_view_open`.
- Behaviour: Display control first asks Commercial / Residential, then lists `catalog_scan("display", true, ...)` entries whose `segment` matches or is `"both"`. Projector control lists `catalog_scan("projector", true, ...)` directly. Untested sets show `*` after the name, the owner's files show `(mine)`. Both open `cmdset_view_open`.

- [ ] **Step 1: Build** with `ufbt` — expected: no warnings.
- [ ] **Step 2: Commit** — `git commit -m "Add Display control and Projector control tools"`

### Task 10: VISCA protocol core

**Files:**
- Create: `flipper-app/core/visca.h/.c`
- Test: `tests/test_visca.c`

**Interfaces:**
- Produces:
  - Framing: `typedef struct { uint8_t buf[16]; uint8_t len; } ViscaFramer; bool visca_framer_feed(ViscaFramer* f, uint8_t byte, uint8_t* packet, uint8_t* len);` (packet ends at `0xFF`; over-long packets are discarded).
  - Client: `size_t visca_build(ViscaCmd cmd, uint8_t addr, uint8_t arg1, uint8_t arg2, uint8_t* out);` with `ViscaCmd` values `ViscaPowerOn, ViscaPowerOff, ViscaPanTiltUp, ViscaPanTiltDown, ViscaPanTiltLeft, ViscaPanTiltRight, ViscaPanTiltUpLeft, ViscaPanTiltUpRight, ViscaPanTiltDownLeft, ViscaPanTiltDownRight, ViscaPanTiltStop, ViscaPanTiltHome, ViscaZoomTele, ViscaZoomWide, ViscaZoomStop, ViscaFocusAuto, ViscaFocusManual, ViscaFocusOnePush, ViscaFocusNear, ViscaFocusFar, ViscaFocusStop, ViscaPresetRecall, ViscaPresetSet, ViscaAddressSet, ViscaInqPower, ViscaInqZoom` (`arg1`/`arg2` = pan/tilt speed, zoom speed 0-7, or preset number).
  - `void visca_describe_reply(const uint8_t* pkt, uint8_t len, char* out, size_t size);` → `"ACK"`, `"Done"`, `"Error: syntax"`, `"Error: buffer full"`, `"Error: cancelled"`, `"Error: no socket"`, `"Error: not executable"`, `"Power: on"`, `"Zoom: 0x1234"`, else hex.
  - Server: `typedef struct { uint8_t address; bool power; uint16_t zoom; bool focus_auto; int16_t pan, tilt; uint32_t received; } ViscaCamera; void visca_camera_init(ViscaCamera* c);` and `size_t visca_camera_handle(ViscaCamera* c, const uint8_t* pkt, uint8_t len, uint8_t* reply, size_t reply_max, char* desc, size_t desc_size);` returns reply bytes (possibly ACK + completion concatenated, or an inquiry answer, or syntax error `90 60 02 FF` style for the camera's address) and a plain-words description.
- Pinned values: address 1 power on = `81 01 04 00 02 FF`; pan/tilt up speeds 8/5 = `81 01 06 01 08 05 03 01 FF`; stop = `81 01 06 01 08 05 03 03 FF` (speeds kept); home `81 01 06 04 FF`; zoom tele variable speed 4 = `81 01 04 07 24 FF`; preset recall 3 = `81 01 04 3F 02 03 FF`; preset set 3 = `81 01 04 3F 01 03 FF`; address set broadcast = `88 30 01 FF`; power inquiry = `81 09 04 00 FF`. Camera at address 1 answers a command with ACK `90 41 FF` then completion `90 51 FF`; answers power inquiry with `90 50 02 FF` (on) or `90 50 03 FF` (off); answers address set `88 30 01 FF` with `88 30 02 FF`; answers an unknown command with `90 60 02 FF`.
- Descriptions to pin: `"Power on"`, `"Pan/tilt up, speed 8/5"`, `"Preset recall 3"`, `"Zoom tele, speed 4"`, `"Unknown: 81 01 7E FF"`.

- [ ] **Step 1: Write the failing tests** `tests/test_visca.c` covering every pinned value and description, the framer (two packets in one feed sequence; a 20-byte junk run without `FF` is discarded), and a server round trip where a packet for address 2 is ignored by a camera at address 1 (returns 0 bytes, description `"For camera 2 (ignored)"`).
- [ ] **Step 2: Run** `make -C tests` — expected: failure.
- [ ] **Step 3: Implement** `visca.c`.
- [ ] **Step 4: Run** `make -C tests` — expected: all pass.
- [ ] **Step 5: Commit** — `git commit -m "Add VISCA protocol core"`

### Task 11: VISCA tool (client and server)

**Files:**
- Create: `flipper-app/tool_visca.c`; Modify: `flipper-app/cereal_tools.c`

**Interfaces:**
- Consumes: Task 10 functions, `serial_port_*`, `app_apply_config`.
- Behaviour: entering applies 9600 8-N-1 and says so (previous settings restored on exit). A first screen picks Client or Server.
  - **Client:** a first screen sets the camera address (1-7) and pan/tilt speed. The control screen has four modes, cycled with a short **OK**: Pan/tilt, Zoom/focus, Presets, Power. In Pan/tilt mode **Up/Down/Left/Right** move while held and stop on release; in Zoom/focus mode Up/Down zoom while held, Left/Right focus near/far while held, hold OK one-push focus; in Presets mode Left/Right choose 0-9, OK recall, hold OK set; in Power mode OK toggles power, hold OK sends address set. The bottom line shows the last reply described.
  - **Server:** shows camera address and received-command count, then the last messages decoded, newest at the bottom; replies sent automatically; OK clears the list; Left toggles decoded/hex.

- [ ] **Step 1: Build** with `ufbt` — expected: no warnings.
- [ ] **Step 2: Commit** — `git commit -m "Add VISCA tool with client and server modes"`

### Task 12: Baud-rate finder

**Files:**
- Create: `flipper-app/core/baudscore.h/.c`, `flipper-app/tool_baud.c`; Modify: `flipper-app/cereal_tools.c`
- Test: `tests/test_baudscore.c`

**Interfaces:**
- Produces: `typedef struct { uint32_t bytes, printable, frame_errors, parity_errors, noise_errors; } BaudSample;` and `uint8_t baud_score(const BaudSample* s);` (0-100); `extern const SerialConfig baud_candidates[];` and `extern const size_t baud_candidate_count;` ordering: all eight rates at 8-N-1, then 7-E-1 and 8-E-1 at 9600, 19200, 38400, 115200.
- Scoring rule (pin in tests): 0 if `bytes < 4`; otherwise start at 100, subtract `min(100, 400 * errors / bytes)` where `errors = frame + parity + noise`, and add up to 10 for a printable ratio over 0.9 (cap at 100). A candidate with a score under 40 is shown as "unlikely".
- Tool: passive by default; listens 1500 ms per candidate (screen shows progress and says "The device must be sending"); shows the top three candidates with scores; OK applies the best to the shared settings. A toggle row "Send a probe at each speed: off" (Left/Right) — when on, sends `\r` once at the start of each candidate's window.

- [ ] **Step 1: Write the failing tests:** `test_too_few_bytes_scores_zero`; `test_clean_text_scores_high` (100 bytes, 95 printable, 0 errors → 100); `test_frame_errors_lower_score` (100 bytes, 10 frame errors → 60); `test_all_errors_scores_zero`; `test_candidate_order` (first is 1200 8-N-1, count 16).
- [ ] **Step 2: Run** `make -C tests` — expected: failure.
- [ ] **Step 3: Implement** `baudscore.c`, then `tool_baud.c`.
- [ ] **Step 4: Run** `make -C tests` and `ufbt` — expected: pass, build clean.
- [ ] **Step 5: Commit** — `git commit -m "Add baud-rate finder"`

### Task 13: README, RJ45 pinout, release build, publish

**Files:**
- Create: `README.md`, `bin/cereal_tools.fap`

**Interfaces:**
- Consumes: everything.
- README sections: what it is (one paragraph); install (copy `bin/cereal_tools.fap` to `SD Card/apps/GPIO/`); the HAT and its RJ45 pinout (TX, RX, GND pin numbers); the seven tools with their controls; the JSON format with a full example and the placeholder table (`{key}`, `{key:2d}`, `{key:2x}`, `{key:2X}`, `{sum}`, `{sum1}`, `{xor}`, `??` in expect, `show` rules); where files live; command sets table (brand, line, source document, status); status table (one row per tool, all "Not yet run on hardware"); Forgejo mirror command (`git remote add forgejo <your Forgejo URL>/Yara162/Flipper-Cereal-Tools.git` then `git push forgejo main`); build instructions (`make -C tests`, `ufbt`); credits (jsmn) and licence.
- RJ45 pinout: search for a published pinout (Rabbit-Labs site, GitHub, forum posts, product photos). If none is found, the README section says "to be measured" and the hand-off message to the owner gives the measuring method: with the app open on Echo, a multimeter on DC volts with the black lead on a Flipper GND pin; the RJ45 pin sitting at roughly -5 to -9 V is the HAT's TX; GND is the pin with continuity to the Flipper's GND; RX is the remaining wired pin (near 0 V, confirm with the Echo tool by sending to it). Never write a guessed pinout.

- [ ] **Step 1: Run** `make -C tests` — expected: every test program passes.
- [ ] **Step 2: Build** `cd flipper-app && ufbt` and copy `dist/cereal_tools.fap` to `bin/`.
- [ ] **Step 3: Write** `README.md`.
- [ ] **Step 4: Commit and push** — `git commit -m "Add README and first release build" && git -c push.negotiate=true push`
