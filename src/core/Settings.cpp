#include "core/Settings.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

Settings settings;

void Settings::load() {
  Settings loaded{};
  HalFile f;
  if (!Storage.openFileForRead("SET", kPath, f)) {
    LOG_INF("SET", "No settings file, using defaults");
    return;
  }
  const size_t n = static_cast<size_t>(f.read(reinterpret_cast<uint8_t*>(&loaded), sizeof(loaded)));
  if (n != sizeof(loaded) || loaded.magic != MAGIC || loaded.version != VERSION) {
    LOG_ERR("SET", "Ignoring invalid settings file");
    return;
  }
  *this = loaded;
  lastBookPath[sizeof(lastBookPath) - 1] = '\0';
  LOG_INF("SET", "Loaded settings (sleep=%u min, refresh=%u)", sleepTimeoutMinutes, refreshEveryNPages);
}

void Settings::save() const {
  Storage.ensureDirectoryExists(kDir);
  HalFile f;
  if (!Storage.openFileForWrite("SET", kPath, f)) {
    LOG_ERR("SET", "Could not write %s", kPath);
    return;
  }
  const size_t n = f.write(this, sizeof(*this));
  if (n != sizeof(*this)) {
    LOG_ERR("SET", "Short settings write");
  }
}

unsigned long Settings::sleepTimeoutMs() const {
  if (sleepTimeoutMinutes == 0) {
    return 0;
  }
  return static_cast<unsigned long>(sleepTimeoutMinutes) * 60UL * 1000UL;
}
