# Deferred decisions

Calls made during the first build that the owner may want to change. Each stands until the
owner says otherwise.

1. **VISCA buttons.** Hold OK switches mode; short OK does each mode's main action; Down saves
   a preset. Zoom and Focus are separate modes, plus a Setup mode for camera address and
   speeds.
2. **Reply timing.** Each command waits for 150 ms of quiet after the reply (up to 1 second),
   so every send takes about 150 ms longer than strictly needed.
3. **Settings inside a command list.** Hold OK opens serial settings.
4. **Unknown fields are errors.** A field the app does not know is rejected, except `notes`,
   and a file with no commands is rejected too.
5. **Missing commands.** Samsung MDC has no volume up/down or HDMI 3/4, and Epson has no
   volume, because none of the documents found had the codes. Samsung offers set-volume to a
   level instead; Epson power state is shown as Epson's number.
6. **BenQ replies.** Write commands do not check a reply. Status readouts look for upper-case
   `POW=` and so on; if a BenQ answers in lower case they show "?" until the file is edited.
7. **Sony power on.** A separate "Allow power on by serial" command is included, because Sony
   requires it before Power on works from standby.
8. **Baud finder probe and score.** The probe is off every time the finder opens, and the
   score is 70% from errors and 30% from readable text.
9. **Back from a brand list** goes to the main menu, not the Commercial/Residential choice.
10. **Emulated VISCA camera** starts powered on and also answers focus-mode, position and
    interface-clear queries.
