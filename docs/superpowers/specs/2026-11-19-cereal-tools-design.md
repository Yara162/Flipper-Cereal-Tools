# Cereal Tools: design

Date: 2026-11-19
Status: approved 2026-11-19

## Intent

Cereal Tools is a Flipper Zero app for AV commissioning over RS-232, used with the
Rabbit-Labs Flip'n Cereal HAT. The owner is an AV programmer (Q-SYS and Crestron first,
Extron some of the time) who wants to test a serial wire and the device at the far end on
site without getting a laptop out: prove a display or projector responds, prove a control
processor's serial port is sending what the program says, and drive or impersonate a
VISCA camera.

It follows the owner's other two Flipper projects, Flipper AV Tools and Flipper Network
Tools: one app with a menu of tools, hardware tests recorded honestly in the README, and
reports saved to the SD card.

## Goals

One Flipper app, "Cereal Tools", with these seven tools on its menu:

1. **Display control.** Power on and off, input switching, volume up and down, and mute
   for common display brands, commercial first and residential for edge cases.
2. **Echo.** Shows what arrives on the line (sent from Q-SYS Designer, SIMPL Debugger or
   similar); OK sends the last message back exactly as received.
3. **VISCA.** Client mode drives a camera. Server mode makes the Flipper act as a camera,
   so the owner can see what a control program sends and the program gets the replies it
   expects.
4. **Sniffer.** Passive listener that logs everything arriving, with timing.
5. **Baud-rate finder.** Works out the speed and framing of a device that is talking.
6. **Projector control.** Power, input, picture mute (shutter), and status queries
   (lamp or light-source hours, errors) for common projector brands.
7. **Custom commands.** Loads command lists from JSON files the owner writes on a laptop
   and copies over with qFlipper; each file sets its serial settings, and each command is
   sent with one press and its reply shown.

Supporting goals:

- One serial settings screen shared by the tools: baud rate, data bits, parity, stop
  bits. Tools that need fixed settings (VISCA) preset them and say so.
- Display and projector command sets ship as JSON files in the same format the owner's
  custom files use, so a wrong command can be fixed on site in qFlipper without a new
  build.
- Published publicly on GitHub as `Yara162/Flipper-Cereal-Tools` (GPL-3.0, like the other
  two projects), and mirrored privately to the owner's own Forgejo server.

## Non-goals

- RS-485, RS-422 and TTL-level serial. The HAT is RS-232 only.
- Network control of any kind (that lives in Flipper Network Tools).
- Hearing both directions of a conversation at once. The HAT has one receive line.
- Hardware flow control (RTS/CTS). Not wired on the HAT, and AV gear rarely uses it.
- A full terminal with free typing. The Flipper keyboard is too slow for it; the custom
  command files cover the need.
- Scripting, macros, conditional sequences.

## Anti-goals (failure even if it technically works)

- **Sending to a device the owner did not choose to send to.** Nothing goes out on the
  line except as the direct result of a button press, with one exception the owner opts
  into: the baud-rate finder's optional active probe, and VISCA server mode's replies.
- **A bad JSON file silently ignored or half-loaded.** The owner must see the file name,
  the line number and what is wrong.
- **An app update overwriting the owner's own files.**
- **Claiming a command set works when it has never been tried on a real device.** Every
  built-in command set is marked untested until the owner confirms it.

## Constraints

- **Hardware.** Flipper Zero on official firmware 1.5.1 (API 88.2), the same as the
  owner's other apps. The Flip'n Cereal HAT sits on the GPIO header, needs no 5 V, and
  works with official firmware's serial apps unmodified, so it almost certainly uses the
  USART on pins 13 (TX) and 14 (RX). This is to be confirmed on the first hardware test;
  if it is the LPUART on pins 15 and 16 instead, only the serial port number changes.
- **Connector.** The HAT has an RJ45 console-style jack. The owner has RJ45-to-DB9
  adapters and null-modem adapters, so cabling is solved on their side; the README should
  still explain when a null-modem is needed. The owner handles cabling and does not need
  wiring guidance, but wants the jack's pinout (which RJ45 pins are TX, RX and ground) in
  the README. Rabbit-Labs does not publish it; the builder looks for it, and if it cannot
  be sourced, gives the owner a quick way to measure it and adds it to the README once
  measured. It is never guessed.
- **Serial port sharing.** The app must disable Flipper's expansion-module service while
  it runs and restore it on exit, as the owner's other serial apps do.
- **Framing.** The Flipper's USART supports 7, 8 (and 9) data bits, none, even or odd
  parity, and 1 or 2 stop bits (`furi_hal_serial_configure_framing`), and reports
  framing, noise, overrun and parity errors on receive.
- **No JSON reader in the Flipper SDK.** The app bundles a small permissively licensed
  parser.
- **Screen.** 128 x 64 pixels, about 25 characters per line in the small font.
- **Accuracy of command sets.** Protocols come from manufacturers' public documents. The
  owner can test only the devices they have, so most command sets stay "untested" for a
  while; the design accepts that and makes it visible.

## Approaches considered

1. **One app, command sets as JSON files (chosen).** One app with a menu of the seven
   tools and one shared serial settings screen. Display and projector command sets ship
   as JSON files in the app's assets folder; the owner's files live in its data folder.
   A wrong command is fixed by editing a file. Costs: a JSON parser in the app, and
   bundled files that can be edited into a broken state (mitigated by clear error
   messages).
2. **One app, command sets compiled in.** Simpler and nothing on the SD card can break,
   but correcting one display command means a new build. Rejected: on site, a wrong
   command needs fixing then and there.
3. **Seven separate apps.** Each is small, but the serial settings screen is repeated
   seven times and the owner's GPIO menu fills up. Rejected.

The owner chose approach 1 and JSON over a plain text format. JSON was chosen because
each command needs several fields, files need serial settings, laptop editors validate
JSON, and Q-SYS and Crestron programmers already work with it.

## Decisions

### Files and folders

- **Built-in command sets:** shipped with the app through `fap_file_assets`, so they land
  in `SD Card/apps_assets/cereal_tools/` and are refreshed when the app is updated.
  - `displays/<brand>_<line>.json`, for example `displays/samsung_mdc.json`
  - `projectors/<brand>_<protocol>.json`
- **Owner's files:** `SD Card/apps_data/cereal_tools/commands/*.json`, never touched by
  updates. The custom commands tool lists every file in that folder. If the folder is
  empty, the app writes one commented example file there so the owner has something to
  copy.
- **Logs and reports:** `SD Card/apps_data/cereal_tools/logs/`, named
  `<tool>_<date>_<time>.txt`.
- **To change a built-in command set permanently,** the owner copies it into their own
  commands folder and edits the copy. Display and projector tools look in both places;
  an owner's file with the same `id` replaces the built-in one. (Builder's call on the
  exact merge rule; this is the intent.)

### JSON file format

One format for built-in and owner files. Proposed shape, refined by the builder:

```json
{
  "format": 1,
  "id": "samsung_mdc",
  "name": "Samsung MDC",
  "category": "display",
  "segment": "commercial",
  "source": "Samsung MDC protocol guide v15.0",
  "tested": false,
  "serial": { "baud": 9600, "data_bits": 8, "parity": "none", "stop_bits": 1 },
  "settings": [
    { "key": "id", "label": "Display ID", "min": 0, "max": 254, "default": 1 }
  ],
  "commands": [
    {
      "name": "Power on",
      "group": "Power",
      "hex": "AA 11 {id} 01 01 {sum}",
      "expect": "AA FF {id} 03 41 11 01"
    },
    {
      "name": "Power on",
      "group": "Power",
      "text": "POWR1   \r",
      "expect_text": "OK\r"
    }
  ]
}
```

Rules:

- A command has either `hex` (bytes written as hex pairs) or `text` (ASCII with `\r`,
  `\n`, `\t`, `\\` and `\xNN` escapes), not both.
- `{name}` placeholders are filled from `settings` the owner sets on the Flipper (display
  ID, VISCA address, and so on). A small set of computed placeholders covers protocol
  checksums; at least `{sum}` (Samsung MDC: low byte of the sum of bytes after the
  header) is needed. The builder adds others a chosen protocol requires and documents
  them.
- `expect` / `expect_text` is optional. When present, the reply is marked OK or
  UNEXPECTED; when absent, the reply is just shown.
- `tested` is `false` for every built-in set until the owner confirms it on hardware; the
  app shows untested sets with a mark in the list. (Designer's call, settled after
  review:) there is no "mark as tested" button in the app. The owner reports results, and
  the builder updates the file's `tested` value and the README, as in the owner's other
  projects. Only the owner's own files are the owner's to mark.
- `format` lets later versions change the shape without misreading old files.
- **Reading values out of replies** (designer's call, settled after review): a command may
  carry a `show` list that pulls values out of the reply, so projector status decoding is
  data in the file and can be fixed on site like everything else. Each entry has a
  `label` and one of:
  - for text replies: `after` (text that comes just before the value) and optionally
    `until` (text or character that ends it), for example
    `{ "label": "Lamp hours", "after": "LAMP=", "until": " " }`;
  - for binary replies: `byte` (offset from the start of the reply) and `length`, read as
    a big-endian number unless `"order": "little"`.
  - An optional `map` turns raw values into words, for example
    `{ "00": "Standby", "01": "On", "02": "Warming up" }`.
  The builder may extend this if a chosen protocol needs it, and documents any additions.
- Errors are reported with file name, line number and a short reason, on screen.

### Shared serial settings

- Baud rates offered: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200.
- Data bits 7 or 8, parity none / even / odd, stop bits 1 or 2.
- The settings screen is reachable from the main menu and from inside each tool.
- Files that state `serial` settings apply them when loaded and say so on screen; the
  owner can still change them.
- The last settings used are remembered between runs (builder's call how).

### Tool behaviour

**Display control**
- Pick segment (Commercial / Residential), then brand and line, then any settings (ID),
  then a list of commands grouped as Power, Input, Volume, Mute.
- Each send shows the bytes sent and the reply, in text or hex (toggle), and OK or
  UNEXPECTED when the file gives an expected reply.
- Brand lines to cover, from public protocol documents (builder researches the exact
  commands and records the source document in each file):
  - Commercial: Samsung MDC; LG commercial (the `ka`/`kb`/`xb` style protocol); Sony
    Bravia Professional displays; Sharp NEC (MultiSync and Sharp PN lines, which use
    different protocols); Planar (the main ASCII protocol); Panasonic professional
    displays; Philips signage (SICP).
  - Residential: LG consumer TVs (same family of commands as LG commercial); Samsung
    consumer TVs where a serial port exists (older EX-Link); Sony consumer Bravia models
    that had RS-232; Sharp Aquos.
  - The builder may drop a line whose public documentation cannot be found, and says so
    in the README rather than guessing commands.

**Echo**
- Shows each message received as a line, text or hex (toggle), newest at the bottom,
  scrollable. A message ends at a line ending or after a quiet gap (builder picks the
  gap; about 50 ms at 9600 baud scales with baud rate).
- **OK** sends the most recent message back, byte for byte.
- **Hold OK** toggles auto-echo, which sends every message back as it arrives.
- Counts messages and bytes. The session can be saved to a log.

**VISCA**
- Defaults to 9600 baud, 8 data bits, no parity, 1 stop bit, and says so.
- **Client:** camera address 1 to 7 (or broadcast address set), D-pad pans and tilts
  while held with adjustable speed, zoom in and out, focus auto / one-push / manual near
  and far, power on and off, home, preset recall and set (0 to 9 at least). Replies
  (ACK, completion, error) shown decoded. Optional inquiries: power state, zoom position.
- **Server:** the Flipper answers as camera address 1 (follows the address set command if
  received), replies with ACK and completion to commands and with plausible values to
  common inquiries (power, zoom position, focus mode, pan/tilt position), and shows each
  received command decoded into plain words ("Pan/tilt up, speed 8/5", "Preset recall
  3", "Zoom tele, variable 4"). Unknown commands are shown as hex and answered with a
  syntax error reply. A counter of received commands.

**Sniffer**
- Passive: never sends anything.
- Lines show a timestamp (time since start), and the data in text or hex. Messages are
  split by line ending or quiet gap, as in Echo.
- Shows framing and parity error counts, which hint at wrong serial settings.
- Logging to the SD card can be started and stopped; the log keeps timestamps and both
  text and hex.

**Baud-rate finder**
- **Passive (default):** listens at each candidate setting for a short while and scores
  it on framing errors, parity errors and how much of what arrives is printable or looks
  like a known protocol. Tries 8-N-1 at every rate first, then 7-E-1 and 8-E-1. Shows the
  best few candidates with a confidence measure, and offers to apply the best one to the
  shared settings.
- Needs the device to be talking. The screen says so.
- **Active probe (opt-in):** the owner can allow the finder to send a carriage return
  (or a probe chosen from a short list) at each rate to provoke a reply. Off unless the
  owner turns it on for that run.

**Projector control**
- Same layout as display control: brand and protocol, then Power, Input, Picture mute,
  Status.
- Status queries decode the reply where the protocol allows: power state, lamp or
  light-source hours, error flags.
- Brands to cover: Epson (ESC/VP21), Panasonic, NEC (Sharp NEC projectors), Sony (serial
  protocol of its business and installation lines), Christie, Barco, BenQ, Optoma,
  Hitachi/Maxell, ViewSonic, Casio. Same rule as displays: drop what cannot be sourced,
  and say so.

**Custom commands**
- Lists the owner's JSON files; picking one applies its serial settings and shows its
  commands, grouped if the file has groups.
- OK sends; the reply is shown, with OK or UNEXPECTED when the command has an expected
  reply. OK on the result screen sends again.
- Uses exactly the same format as the built-in sets.

### Saving

- Echo and Sniffer logs on request. Display, projector and custom tools keep a short
  in-memory history of sends and replies that can be saved.
- Every log starts with the date, the tool and the serial settings in use.

### Testing and honesty

- Protocol pieces that do not need hardware are tested on a PC: the JSON reader, the
  placeholder and checksum filling, VISCA encoding and decoding, the baud-rate scoring,
  and every built-in command file (it loads, every command renders to bytes).
- The README carries a status table like the owner's other projects, one row per tool and
  one per command set, each starting as "not yet run on hardware" and updated only when
  the owner reports a result.

### Repository and publishing

- Local repository `flipper-cereal-tools`, pushed to GitHub `Yara162/Flipper-Cereal-Tools`
  (the owner creates the empty repository). Public.
- Forgejo: the owner's server is not reachable from the build environment, so the README
  (or a note to the owner) gives the one command to add it as a second remote and push.
- Licence GPL-3.0, copyright Yara162, matching the owner's other projects. Bundled
  third-party code keeps its own licence notice.
- Commit author `Yara162` with GitHub's no-reply address, as in the other repositories.

### Release plan (designer's call, settled after review)

- **First release:** the serial core, settings screen, file loading and all seven tools,
  with a first batch of command sets led by the owner's hardware on hand:
  - **Samsung MDC**, for the Samsung QB series displays at the owner's work (binary
    protocol with a checksum).
  - **Sony Bravia**, for the owner's Sony Bravia A1E at home, which has a serial port the
    owner already uses for testing. The builder researches Sony's serial protocol for
    Bravia models of that generation.
  - **BenQ projector** protocol, for the owner's BenQ projector (ASCII, with status
    queries).
  - **VISCA**, tested on the owner's Marshall cameras. The owner's rule: if it works on a
    Marshall, VISCA counts as working for the cameras they use.
  - **LG** (ASCII) and **Epson ESC/VP21** (projector status decoding), untested, to prove
    the file format against more protocol shapes.
- **Later releases:** the remaining command sets in batches, each researched, PC-tested
  and marked untested until the owner reports on hardware.

## Left to the builder

- Exact command bytes for every brand line, researched from public documents, with the
  source recorded in each file.
- Which JSON parser to bundle (small, permissive licence).
- Screen layouts within the 128 x 64 display, following the style of the owner's other
  apps.
- Quiet-gap timing for splitting messages, and how long the baud-rate finder listens at
  each setting.
- How the last-used serial settings are stored.
- The exact rule for an owner file replacing a built-in one.
- Order of building the tools; a sensible order is serial core and settings, Echo,
  Sniffer, Custom commands (which proves the file format), Display control, Projector
  control, VISCA, Baud-rate finder.
