#pragma once

#include <cstddef>

// Flash an ESP32 app image from the SD card into the next OTA slot.
namespace sdUpdate {
struct Check {
  bool ok = false;
  size_t size = 0;
  const char* error = "invalid firmware";
};

Check inspect(const char* path);

// Writes the image. Does not restart. Does not paint (SD and the panel share SPI).
bool flash(const char* path);

// Valid until the next inspect() or flash() call.
const char* lastError();
}  // namespace sdUpdate
