# Changelog

## v1.0.0 — First release

Dual-board (Meshnology ESP32-S3 rotary / DIYmalls ESP32-C3 touch) MTG life
counter built on LVGL 8.3, with:

- 1–4 player life counter with adaptive layout, `+`/`-` per player
- Setup screen (player count, starting life) and a swipe-to-reach settings
  menu (d20 roller, new game, random first player, brightness, theme, LED
  settings, extra-counter toggles, table sync)
- Optional poison, energy, storm, and commander-damage tracking, with a
  per-player detail view (long-press a life total); commander damage comes
  directly off life like any other hit
- Ambient WS2812 LED strip support on the Meshnology board: on/off,
  brightness, a cyclable base color, and optional per-player "turn colors"
  that switch with the active player
- **Table sync**: link multiple boards over ESP-NOW into one shared
  scoreboard — one device hosts, others join, and life/poison/energy/
  commander-damage/storm/turn-colors stay in sync across every linked
  device. See the README's "Table sync" section for setup and v1
  limitations.

See the README for build instructions, board pinouts, and known
simplifications.
