# Lazahata

XTCH reader for the Xteink X3. Books are `.xtch` files on the SD card: pre-rendered 2-bit page bitmaps. This firmware blits them. It does not lay out text, scale pages, or read `.xtc`.

The panel is 792×528 landscape. UI and pages are portrait 528×792.

## Layout

```
src/main.cpp          boot + loop
src/core/             activity stack, input, settings, power
src/screens/          home, browser, reader, settings
src/platform/         chip workarounds
lib/hal               hardware wrappers (still CrossPoint-shaped)
lib/Gfx               portrait framebuffer + UI text
lib/Xtch              XTCH container + page blit
lib/EpdFont           Ubuntu UI fonts
reference/            CrossPoint submodule (lookup only)
freeink-sdk/          display and board support
```

Settings and per-book progress live in `/.lazahata` on the SD card.

## Build

One firmware binary per device. Default env is `x3`.

```bash
pio run              # env:x3
pio run -t upload
pio device monitor
```

Later devices get their own env (`pio run -e x4`, …), not a combined binary.

Optional `platformio.local.ini` for a local upload port (gitignored).

`reference/` is the CrossPoint Reader tree. Do not build it as this product.

```bash
git submodule update --init --recursive
```
