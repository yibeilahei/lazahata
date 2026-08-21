# Lazahata

Dedicated XTCH reader firmware. SD-only. Books are pre-rendered 2-bit page bitmaps; this binary blits them. `.xtc` is not supported.

Typography, fonts, and layout belong on the host converter. This X3 build expects 528×792 pages. The firmware does not scale.

## Layout

```
src/main.cpp          boot + loop
src/core/             activity stack, input, settings, power
src/screens/          home, browser, reader, settings
src/platform/         chip workarounds
lib/hal               hardware wrappers
lib/Gfx               framebuffer + UI text
lib/Xtch              XTCH container + page blit
lib/EpdFont           Ubuntu UI fonts
reference/            CrossPoint submodule (lookup only)
```

## Build

One firmware binary per device. Default is the X3 (792×528).

```bash
pio run              # env:x3
pio run -t upload
```

Later devices get their own env (`pio run -e x4`, …), not a combined binary.

```bash
pio device monitor
```

Optional `platformio.local.ini` for a local upload port (gitignored).

The original CrossPoint Reader tree is a git submodule at [`reference/`](reference/). Do not build that as the product.

```bash
git submodule update --init --recursive
```
