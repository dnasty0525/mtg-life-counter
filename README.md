# MTG Life Counter (LVGL, dual-board)

A Magic: The Gathering life counter for your two round 240x240 displays:

- **Meshnology ESP32-S3** — this is an Elecrow CrowPanel 1.28" ESP32-S3
  Rotary Display: GC9A01 panel + CST816D capacitive touch + a physical
  rotary encoder with push-button, plus a WS2812 ambient RGB LED strip,
  all present on the same board. Both touch (tap `+`/`-`) and the knob
  (turn to adjust, click to select/confirm, long-press for dice) work.
- **DIYmalls ESP32-C3** (ESP32-2424S012C-I-Y(B)) — capacitive touch only, direct tap `+`/`-` buttons

One shared LVGL codebase drives both. Board differences (pins, which input
device exists) live entirely in `boards/board_config.h` and
`src/input_driver.*`; the screens in `src/screens/` don't know or care
which board they're running on.

## Features (v2)

- New-game setup: pick 1–4 players and starting life, then start
- Adaptive life-counter layout (1 big counter / 2 halves / 3 or 4 around
  the dial) with `+`/`-` per player
- **Swipe left** on the counter screen to reach the **menu screen**
  (swipe right, or the Back button / encoder long-press, to return). It
  holds everything that isn't a life total:
  - d20 dice roller (also still reachable via encoder long-press on the
    counter screen)
  - New Game button
  - Random first-player picker
  - Screen brightness slider
  - Theme cycle button (4 accent-color presets)
  - Ambient LED section (Meshnology board only — hidden automatically on
    boards without the LED strip): on/off switch, brightness slider, a
    cycle button for the strip's base color (8 presets), a **Turn
    Colors** switch, and one color-cycle button per active player to
    assign their turn color. With Turn Colors on, the strip switches to
    the current player's color the moment the encoder selects them
    (touch-only boards don't have a concept of "selected player", so this
    only does anything on the rotary board)
  - Switches to turn on extra counters: **Poison**, **Energy**, **Storm**
    (shared per-turn count with its own +/-/reset), **Commander Damage**
  - **Multiplayer (Sync)** section: link several of these boards at the
    table so every player's life/poison/energy/commander-damage/storm
    shows the same on every screen. See "Table sync" below.
- **Long-press a player's life total** (touch, either board) to open
  their detail view — poison, energy, and commander damage taken from
  each opponent, each with its own +/- stepper. Only the sections you've
  turned on in the menu show up. Commander damage comes directly off that
  player's life total (same as any other hit) and the counter screen
  underneath updates live as you adjust it.
- Small poison/energy badge in the corner of each player's box on the
  main counter screen once those counters have a nonzero value
- Rotary board: turning the knob adjusts whichever player is "selected"
  (ring highlight); click cycles selection; long-press opens the dice
  roller from the counter screen, or starts the game from setup; on the
  menu screen the knob scrolls the list instead, and long-press goes back

## Project layout

```
platformio.ini          two build envs: meshnology_s3 / diymalls_c3
boards/board_config.h    per-board pin map + capability flags (BOARD_HAS_TOUCH / BOARD_HAS_ENCODER)
include/lv_conf.h        LVGL config (240x240, 16-bit color)
src/display_driver.*     LovyanGFX panel setup + LVGL flush callback
src/input_driver.*       touch -> LVGL pointer indev; encoder -> semantic events
src/app_state.*          game state: player life totals, extra counters, selected player, current screen
src/led_driver.*         ambient WS2812 LED strip control + its on/off/brightness/color/turn-color
                         runtime state (no-op on boards without a strip)
src/led_colors.*         named RGB palette shared by the ambient-color and per-player color pickers
src/net/sync_protocol.h  wire format (packed structs) for table sync over ESP-NOW
src/net/table_sync.*     ESP-NOW host/joiner logic: beacons, join/leave, action relay, state broadcast
src/ui/theme.*           shared colors + the cyclable runtime accent-color theme
src/screens/setup_screen.*    player count / starting life picker
src/screens/counter_screen.*  the main life-total screen
src/screens/menu_screen.*     settings/utility screen (dice, new game, random first player,
                               brightness, theme, LED, extra-counter toggles, multiplayer sync)
                               — swipe left to reach it
src/screens/dice_overlay.*    d20 roller modal
src/screens/player_detail_overlay.*  per-player poison/energy/commander-damage view (long-press a player)
tools/pin_finder/         standalone GPIO-scanner sketch (kept for any future board with unknown pins)
```

## Building

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

```
pio run -e diymalls_c3              # build for the touch board
pio run -e diymalls_c3 -t upload    # flash it

pio run -e meshnology_s3            # build for the rotary board
pio run -e meshnology_s3 -t upload  # flash it
```

Dependencies (LVGL, TFT_eSPI, LovyanGFX) are pulled automatically by
PlatformIO's `lib_deps` on first build.

Uses the `huge_app.csv` partition scheme (set in `platformio.ini`) instead
of each board's default — table sync's WiFi/ESP-NOW stack doesn't fit in
the default scheme's ~1.25MB app slot (that scheme reserves a second slot
for OTA updates, which this project doesn't use). Needs a 4MB+ flash chip,
which both boards have.

## Critical gotcha: display power-enable pins

The Meshnology/CrowPanel S3 board needs GPIO1 and GPIO2 driven HIGH before
the display panel will respond to *anything* — they're power-enable rails
(likely a level-shifter/LDO enable) for the panel's logic supply. This
isn't documented in the wiki or pin list anywhere; it only showed up in
the factory source code's `setup()`. Without it, the backlight can still
glow (it's a separate line) while the GC9A01 controller itself has no
power, which looks exactly like "nothing shows up" even though SPI
writes are happening. `display_driver_init()` in `src/display_driver.cpp`
handles this automatically via `PIN_DISPLAY_PWR_EN1`/`PIN_DISPLAY_PWR_EN2`
in `boards/board_config.h`.

## Board pinout status

Both boards' pins are filled in and confirmed — DIYmalls C3 from your
earlier working flash, Meshnology S3 from the
[Elecrow CrowPanel 1.28" wiki](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html)
(this is an Elecrow CrowPanel board, ESP32-S3R8 with 8MB PSRAM). Nothing
left to trace with `pin_finder` for either board right now.

One thing worth checking once you have it running: `ENC_STEPS_PER_DETENT`
in `src/input_driver.cpp` assumes a common 4-pulses-per-detent encoder. If
turning the knob one physical "click" moves the life total by more or
less than 1, adjust that constant.

## Adding commander-art backgrounds (the feature you asked about)

LVGL images have to be compiled in as C arrays — you can't just drop a
PNG in `data/` and have it load at runtime. To add a background:

1. Convert your art with the [LVGL online image converter](https://lvgl.io/tools/imageconverter)
   (color format: `True color`, output: `C array`), sized to fit the
   region you want it behind (e.g. ~96x72 per player, or 240x240 for a
   1-player full-screen background).
2. Save the generated `.c` file into `data/` and `#include` it where
   needed (e.g. in `counter_screen.cpp`).
3. In `build_player_widget()`, add an `lv_img_create(cont)` behind the
   life label using `LV_IMG_DECLARE(your_image)` + `lv_img_set_src()`,
   and move the life label/buttons in front of it (LVGL draws children in
   creation order, so create the image first).

This wasn't wired up in v1 since it needs your actual art assets to be
useful — the hook point above is exactly where to add it.

## Table sync

Menu screen → **Multiplayer (Sync)** lets several of these boards (either
board type, any mix of the two) share one game: one device **hosts**, the
others **join** it, and from then on every linked device shows the same
life totals, poison/energy, commander damage, storm count, and LED
turn-color assignments — the host is the single source of truth, and
joiners' button presses become requests the host applies and broadcasts
back out.

- On the device that will hold "the" scoreboard: **Host Table**.
- On every other device: **Join Table (Scan)**, wait for it to appear in
  the list (beacons go out roughly once a second), tap it. That device
  jumps straight to the life-total screen once the host accepts.
- **Leave Table** / **Stop Hosting** disconnects. A joiner that loses the
  host (out of range, host stopped) shows "Lost connection to host" after
  ~5 seconds rather than silently going stale.
- Only the host can start a **New Game** — that button is hidden on
  joiners. Everything else (adjusting life, poison/energy, commander
  damage, storm, the extra-counter toggles) works identically from any
  device.

Under the hood this rides on **ESP-NOW**, not classic Bluetooth — see the
comment at the top of `src/net/sync_protocol.h` for why. No pairing step,
no phone app; it's device-to-device over the WiFi radio both boards
already have.

**Testing with one device:** ESP-NOW needs two physical radios talking to
each other, so this can't be exercised solo in a simulator — you'll want
at least two flashed boards (any mix of the two board types) within WiFi
range of each other to see it work end to end.

**v1 limitations, worth knowing before you rely on this at a table:**
- Table and device names are fixed strings ("MTG Table" / "Player") —
  there's no on-device keyboard yet to type your own, so with more than
  one table hosting nearby the scan list won't tell them apart by name.
  (`host_btn_cb`/`join_btn_cb` in `menu_screen.cpp` are the one-line spots
  to wire in real names once there's a text-entry UI.)
- The discovered-table list and the "Hosting (N joined)" status only
  refresh when you tap **Refresh List** / reopen the menu — they don't
  live-update while you're looking at them.
- No passphrase/encryption on the ESP-NOW traffic — anyone else's
  ESP-NOW device in range that happens to guess a matching protocol magic
  could in principle interact with a hosted table. Fine for a living-room
  game night, not something to expose at an event with strangers'
  hardware around.
- If two devices' New Game player counts disagree, joining always wins —
  a joiner's own local player count is overwritten by whatever the host
  sends the moment it joins.

## Known v2 simplifications

- 3-player layout uses horizontal-band-ish positions rather than true
  120° pie wedges — readable, but a true wedge layout (via `lv_canvas` or
  masked arcs) would look nicer. Left as a v2 idea.
- Two-player layout doesn't rotate the top player's number 180° (so
  players sitting across from each other can each read their own number
  right-side-up), which most polished life-counter apps do. LVGL labels
  can't be rotated directly pre-v9; doing this needs rendering the label
  to a canvas and rotating that. Flagged here rather than skipped
  silently.
- No persistence — life totals (and the extra counters) reset on
  reboot/power-loss, though theme, the extra-counter on/off switches, and
  the LED settings (ambient color, Turn Colors, per-player color
  assignments) are meant as session-to-session preferences and are simply
  kept in RAM until power is lost (fine for a single sit-down; shout if
  you want values saved to flash between games/boots).
- Table sync state (hosting/joined/scanning) is also RAM-only — a reboot
  drops a device out of its table and it has to Host/Join again.
- Storm count is a single shared counter (matches the paper rule — reset
  it yourself each turn from the menu screen) rather than tracked per
  player.
- The menu screen's brightness slider calls `gfx.setBrightness()`
  directly — it isn't saved anywhere, so it resets to full brightness on
  reboot along with everything else above.

## Contributing

Issues and pull requests are welcome — board pin reports for other
round-display ESP32 boards, layout/UI polish, and progress on any of the
v1/v2 items listed above are all useful. There's no formal process here;
open an issue describing what you're seeing (board, PlatformIO env, and a
serial log if it's a boot/crash problem) or send a PR.

## License

[GPL-3.0](LICENSE) — see the LICENSE file for the full text. Anyone can
use, study, and modify this code; anyone who distributes a modified
version (including on a device they sell) must also make that version's
source available under GPL-3.0.
