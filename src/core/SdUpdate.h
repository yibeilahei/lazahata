#pragma once

#include <cstddef>

class Gfx;

// Flash an ESP32 app image from the SD card into the next OTA slot.
namespace sdUpdate {
constexpr const char* kPath = "/update.bin";

struct Check {
  bool ok = false;
  size_t size = 0;
  const char* error = "invalid firmware";
};

Check inspect(const char* path);

using ProgressFn = void (*)(size_t written, size_t total, void* ctx);
// Writes the image. Does not restart. Returns false on error; error string in inspect-style logs.
bool flash(const char* path, ProgressFn onProgress, void* ctx);

// If /update.bin is present, flash it, delete it, and restart. True if restarting.
bool tryApply(Gfx& gfx);
}  // namespace sdUpdate
