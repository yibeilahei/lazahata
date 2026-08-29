# Lazahata

XTCH reader for the Xteink X3. Books are `.xtch` files on the SD card: pre-rendered 2-bit page bitmaps. This firmware blits them. It does not lay out text, scale pages, or read `.xtc`.

The panel is 792×528 landscape. UI and pages are portrait 528×792.

## Layout

```
src/main.cpp          boot + loop
src/core/             screen stack, input, settings, power, list UI
src/screens/          home, browser, reader, chapters, settings, update
src/platform/         chip workarounds
lib/hal               hardware wrappers (still CrossPoint-shaped)
lib/Gfx               portrait framebuffer + UI text
lib/Xtch              XTCH container + page blit
lib/EpdFont           Ubuntu UI fonts
reference/            CrossPoint submodule (lookup only)
freeink-sdk/          display and board support
```

Settings and per-book progress live in `/.lazahata` on the SD card.
Boot and runtime logs go to the Serial console only (no SD log file).

Firmware update from SD (same idea as CrossPoint):

- Settings → **Update firmware** → pick a `.bin` → confirm.
- Or copy `.pio/build/x3/firmware.bin` to the SD root as `update.bin` and boot.
- Recovery picker: hold **up + power** at wake (CrossPoint's X3 chord). Back stays in
  the picker.

The stock X3 left+power updater is gone once this firmware is installed.

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
