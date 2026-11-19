# Flipper Cereal Tools

A Flipper Zero app with seven RS-232 tools for AV commissioning, made for the
[Rabbit-Labs Flip'n Cereal HAT](https://rabbit-labs.com/product/flipper-zero-flipn-cereal-rs232-by-rabbit-labs/).
Test a serial line and the device at the far end without a laptop: prove a display or
projector responds, see exactly what a Q-SYS or Crestron program sends, and drive or
impersonate a VISCA camera.

| Tool | What it does |
|---|---|
| Display control | Power, inputs, volume and mute for common commercial and residential displays |
| Projector control | Power, inputs, picture mute, and status: power state, lamp hours, errors |
| Custom commands | Your own command lists, written as JSON files on your computer |
| Echo | Shows what arrives; OK sends the latest message back exactly as received |
| Sniffer | Listens only, timestamps every message, logs to the SD card |
| VISCA camera | Client: drive a camera. Server: act as a camera and decode what a controller sends |
| Baud-rate finder | Works out a device's speed and framing, with probes and a cable check |

## Install

Copy [`bin/cereal_tools.fap`](bin/) to `SD Card/apps/GPIO/` with qFlipper, then open
Apps → GPIO → Cereal Tools. Built for Flipper firmware 1.5.1 (API 88.2); rebuild with
[ufbt](https://github.com/flipperdevices/flipperzero-ufbt) for other versions.

The app's built-in command sets are bundled inside the `.fap` and unpack themselves to
`SD Card/apps_assets/cereal_tools/` the first time it runs.

## The HAT

The Flip'n Cereal sits on the Flipper's GPIO header and converts its serial pins to
RS-232 on an RJ45 jack. It needs no 5 V and works with official firmware. The app uses
the Flipper's USART (GPIO pins 13 TX and 14 RX).

### RJ45 pinout

| RJ45 pin | Signal |
|---|---|
| to be measured | TX (from the Flipper) |
| to be measured | RX (to the Flipper) |
| to be measured | Ground |

Rabbit-Labs does not publish the pinout. To measure it: open Echo (so the port is active)
and set a multimeter to DC volts with the black lead on a Flipper GND pin. The RJ45 pin
sitting at about -5 to -9 V is TX; the pin with continuity to the Flipper's GND is ground;
RX is the remaining wired pin, which you can confirm by sending to it and watching Echo.

### Wiring

Through an RJ45-to-DB9 adapter, if nothing arrives at all, add or remove a null-modem
adapter: the HAT's TX has to meet the device's RX.

## Using the tools

Every tool's screen shows its main buttons on the bottom line. Back always goes back.

**Serial settings** (main menu, and hold OK inside a command list): baud rate 1200 to
115200, data bits 7 or 8, parity none/even/odd, stop bits 1 or 2. Saved between runs.
Files that state their own settings apply them when opened.

**Display control** asks Commercial or Residential, then lists the command sets. **Projector
control** lists projector sets. **Custom commands** lists your own files. All three open the
same command screen:

- settings rows at the top (display ID, volume level): Left/Right change them
- commands grouped as Power, Input, Volume, Mute: OK sends
- the result shows the bytes sent, the reply as text or hex (Left toggles), OK or
  UNEXPECTED when the file says what reply to expect, and any values read from the reply;
  the app keeps listening until the line has been quiet for 150 ms (up to 1 second)
- OK on the result sends again; hold Right on the list saves the session to a log

A `*` after a name means that command set has not yet been confirmed on real hardware.
`(mine)` marks your own files.

**Echo**: OK sends the latest message back byte for byte, hold OK turns on auto-echo (every
message sent back as it arrives), Left toggles hex, Right opens serial settings without
losing the list, hold Right saves a log, Up/Down scroll.

In Echo and the Sniffer, a message longer than 256 bytes is split; a `+` at the end of a line
(and "continues" in the log) means the next line carries on the same message.

**Sniffer**: never sends anything. Each message shows the time since start. OK starts and
stops logging, Left toggles hex, Right opens serial settings without losing the capture.
Framing and parity error counts on the bottom line hint at wrong serial settings.

**VISCA camera** uses 8-N-1 at 9600 baud by default; Left/Right on the first screen picks
19200, 38400 or 115200 instead. Your own settings come back when you leave.

- Client: hold OK to switch mode.
  - Pan/tilt: arrows move while held, OK homes.
  - Zoom: Up/Down zoom tele/wide while held.
  - Focus: Left/Right focus near/far while held (switches the camera to manual focus), OK
    returns to auto focus, Up does a one-push focus.
  - Presets: Left/Right pick 0-9, OK recalls, Down saves.
  - Power: OK on/off, Up sends address set, Down asks the power state.
  - Setup: camera address and speeds.
- Server: answers as camera 1 (or whatever number an address set gives it), replies like a
  camera, and shows each command in plain words: "Pan/tilt up, speed 8/5", "Preset recall 3".
  Unknown commands show as hex and get a syntax-error reply. Left toggles hex, OK clears.

**Baud-rate finder** listens for 1.5 seconds at each of 16 settings (all eight speeds at
8-N-1, then 7-E-1 and 8-E-1 at common speeds) and shows the three best with a score out of
100: mostly how few framing and parity errors arrived, partly how much of it is readable
text. OK applies the best.

The device must be talking. Most displays and projectors only answer when asked, so pick a
**Probe** (Left/Right on the first row) and the finder sends it at the start of each setting:

| Probe | Sends |
|---|---|
| Off (default each time) | nothing |
| Carriage return | `\r` |
| BenQ status | `\r*pow=?#\r` |
| Epson status | `PWR?\r` |
| LG status | `ka 01 FF\r` |
| Samsung MDC status | `AA 11 01 00 12` |
| Sony status | `83 00 00 FF FF 81` |
| VISCA power | `81 09 04 00 FF` |

**Check cable** proves the wiring before you blame the device. Join TX and RX at the far end
of the cable (DB9 pins 2 and 3), then choose Check cable: it sends
`CEREAL CABLE CHECK 0123456789` at 9600 8-N-1 and reports PASS (everything came back),
PARTLY (some came back: loose contact or noise), GARBLED (something came back but not the
pattern), or NOTHING CAME BACK (jumper, ground, or TX/RX pins). Your saved settings are put
back afterwards.

## Files on the SD card

| What | Where |
|---|---|
| Your command files | `apps_data/cereal_tools/commands/*.json` |
| Logs | `apps_data/cereal_tools/logs/<tool>_<date>_<time>.txt` |
| Serial settings | `apps_data/cereal_tools/settings.txt` |
| Built-in command sets | `apps_assets/cereal_tools/displays/` and `projectors/` (replaced on update) |

The first time you open Custom commands with an empty folder, the app copies an example,
`my_device.json`, into it. To change a built-in set permanently, copy it into your
commands folder and edit the copy: a file of yours with the same `id` replaces the
built-in one, and app updates never touch your folder.

## Command file format

```json
{
  "format": 1,
  "id": "my_display",
  "name": "Lobby display",
  "category": "display",
  "segment": "commercial",
  "serial": { "baud": 9600, "data_bits": 8, "parity": "none", "stop_bits": 1 },
  "settings": [
    { "key": "id", "label": "Display ID", "min": 1, "max": 254, "default": 1 }
  ],
  "commands": [
    { "name": "Power on", "group": "Power", "hex": "AA 11 {id} 01 01 {sum1}", "expect": "AA FF {id} 03 41 11" },
    { "name": "Input HDMI 1", "group": "Input", "text": "xb {id:2x} 90\r", "expect_text": "OK" },
    { "name": "Lamp hours", "group": "Status", "text": "LAMP?\r",
      "show": [ { "label": "Lamp hours", "after": "LAMP=", "until": "\r" } ] }
  ]
}
```

| Field | Meaning |
|---|---|
| `format` | Always `1` |
| `id` | Unique name; a file of yours with a built-in set's `id` replaces it |
| `name` | Shown in lists (defaults to `id`) |
| `category` | `display` or `projector` to appear under those tools; anything else for Custom commands only |
| `segment` | `commercial`, `residential` or `both` (displays) |
| `serial` | Optional; applied when the file is opened |
| `settings` | Up to 4 values you set on the Flipper, used as `{key}` in commands |
| `commands` | Up to 48. Each has `name`, optional `group`, and either `hex` or `text` |
| `expect` / `expect_text` | Optional reply to look for, as hex or text; shown as OK or UNEXPECTED |
| `show` | Optional values to read out of the reply (below) |

A field the app does not know is an error, so a misspelt name never goes unnoticed. Use
`notes` for your own comments; it is the one extra field allowed. A file needs at least one
command.

**Text** may use `\r`, `\n`, `\t`, `\\` and `\xNN` for a byte by its hex value; in a JSON
file each backslash is written twice (`"\\x02"`), except `\r` `\n` `\t`, which JSON already
understands. Write `{{` for a literal `{`.

**Placeholders**

| Placeholder | Becomes |
|---|---|
| `{key}` | The setting's value: one byte in hex commands, a decimal number in text |
| `{key:2d}` | Text only: zero-padded decimal, width 1 to 4 |
| `{key:2x}` / `{key:2X}` | Text only: lower- or upper-case hex, width 1 to 4 |
| `{sum}` | Hex only: low byte of the sum of every byte before it |
| `{sum1}` | Hex only: the same, skipping the first byte (Samsung MDC) |
| `{xor}` | Hex only: XOR of every byte before it |
| `??` | In `expect` hex only: any byte |

**Reading values** with `show`: for text replies, `after` is the text just before the value
and `until` the text that ends it. For binary replies, `byte` is the position (from 0) and
`length` 1 to 4 bytes, big-endian unless `"order": "little"`. An optional `map` turns raw
values into words, for example `{ "00": "Standby", "01": "On" }`.

If a file has a mistake, the app lists it with `!` and shows the file name, line number and
reason when you open it.

## Command sets included

| Set | Source document | What's missing |
|---|---|---|
| Samsung MDC | Samsung MDC RS232 protocol v2.1; Elipse MDC driver guide | Volume up/down (set a level instead), HDMI 3 and 4: codes not in the sources |
| Sony Bravia | Sony BRAVIA Professional serial control command list | |
| LG | LG RS-232C external control guide | Some LG models use `kb` instead of `xb` for inputs |
| BenQ (projector) | BenQ TK850 RS232 Control Guide 1.01 | Expected replies for write commands |
| Epson ESC/VP21 (projector) | Epson ESC/VP21 Command User's Guide for Business Projectors | Volume; power status is shown as Epson's number |

Sony Bravia sets need **Allow power on by serial** sent once, or Power on is ignored in
standby. Turn on RS-232C control in the TV's settings first.

More brands follow in later releases.

## Status

| Item | Status |
|---|---|
| Echo | Not yet run on hardware |
| Sniffer | Not yet run on hardware |
| Custom commands | Not yet run on hardware |
| Display control | Not yet run on hardware |
| Projector control | Crashes with "Out of memory" on hardware (see Known issues) |
| VISCA client and server | Not yet run on hardware |
| Baud-rate finder | Not yet run on hardware |
| Samsung MDC set | Not yet run on hardware |
| Sony Bravia set | Not yet run on hardware |
| LG set | Not yet run on hardware |
| BenQ set | Not yet run on hardware |
| Epson set | Not yet run on hardware |

The protocol code (JSON reader, command building and checksums, message splitting, VISCA,
baud scoring, and every built-in command file) is tested on a PC.

## Known issues

| Issue | Status |
|---|---|
| Opening **Projector control** crashes the Flipper with "Out of memory" | Open, found on hardware. Building the list of command files needs more memory than is free at that point. Until it is fixed, avoid Projector control. |

## Build

```bash
make -C tests
```

```bash
cd flipper-app && ufbt
```

The first runs the PC tests; the second builds `flipper-app/dist/cereal_tools.fap`.

## Private mirror

To keep a copy on your own Forgejo server, add it as a second remote and push:

```bash
git remote add forgejo https://<your Forgejo server>/Yara162/Flipper-Cereal-Tools.git
```

```bash
git push forgejo main
```

## Credits

- JSON parsing: [jsmn](https://github.com/zserge/jsmn) by Serge Zaitsev (MIT licence, notice in `flipper-app/lib/jsmn/LICENSE`).
- Protocol details from the manufacturers' public documents listed above.

## Licence

Copyright (C) 2026 Yara162. Released under the [GNU General Public License v3.0](LICENSE).
