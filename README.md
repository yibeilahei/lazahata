# CrossXTCH

Dedicated XTCH reader firmware. SD-only. Books are pre-rendered 2-bit page bitmaps; this binary blits them. `.xtc` is not supported.

Typography, fonts, and layout belong on the host converter. This X3 build expects 528×792 pages. The firmware does not scale.

## Build

One firmware binary per device. Default is the X3 (792×528).

From this directory:

```bash
pio run              # env:x3
pio run -t upload
```

Later devices get their own env (`pio run -e x4`, …), not a combined binary.

USB serial monitor:

```bash
pio device monitor
```

Optional `platformio.local.ini` for a local upload port (gitignored).

The original CrossPoint Reader tree is a git submodule at [`reference/`](reference/) for lookup. Do not build that as the product.

```bash
git submodule update --init --recursive
```

## What this tree is

New firmware, not a trimmed CrossPoint. Copied pieces (MIT, CrossPoint / FreeInk):

- `lib/hal` — display, GPIO, SD mutex, power, clock, frontlight, tilt
- `lib/Logging`, `lib/Memory`, `lib/Utf8`
- Ubuntu 12pt UI fonts
- XTCH on-disk layout (`lib/Xtc`)

Everything else (blit, menus, activity loop) is new.

## First milestone

Open an `.xtch` from the SD card, turn pages, sleep, resume at the same page.
