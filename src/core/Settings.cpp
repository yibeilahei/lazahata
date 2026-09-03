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
  if (n < 10 || loaded.magic != MAGIC || loaded.version < kMinVersion || loaded.version > VERSION) {
    LOG_ERR("SET", "Ignoring settings file (n=%u magic=0x%08lX ver=%u, need %u/0x%08lX/%u-%u)",
            static_cast<unsigned>(n), static_cast<unsigned long>(loaded.magic), loaded.version,
            static_cast<unsigned>(sizeof(loaded)), static_cast<unsigned long>(MAGIC), kMinVersion, VERSION);
    return;
  }
  if (loaded.version < VERSION) {
    loaded.trueSleepMinutes = 0;
    loaded.version = VERSION;
  }
  *this = loaded;
  lastBookPath[sizeof(lastBookPath) - 1] = '\0';
  if (sleepTimeoutMinutes != 0 && sleepTimeoutMinutes != 1 && sleepTimeoutMinutes != 2 &&
      sleepTimeoutMinutes != 3) {
    sleepTimeoutMinutes = 3;
  }
  if (trueSleepMinutes != 0 && trueSleepMinutes != 10 && trueSleepMinutes != 20 &&
      trueSleepMinutes != 30) {
    trueSleepMinutes = 10;
  }
  LOG_INF("SET", "Loaded light=%u min sleep=%u min refresh=%u night=%u tilt=%u last='%s'", sleepTimeoutMinutes,
          trueSleepMinutes, refreshEveryNPages, nightMode, tiltPageTurn, lastBookPath);
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
    LOG_ERR("SET", "Short settings write (%u of %u)", static_cast<unsigned>(n), static_cast<unsigned>(sizeof(*this)));
    return;
  }
  LOG_DBG("SET", "Saved light=%u sleep=%u refresh=%u night=%u tilt=%u last='%s'", sleepTimeoutMinutes,
          trueSleepMinutes, refreshEveryNPages, nightMode, tiltPageTurn, lastBookPath);
}

namespace {
unsigned long minutesToMs(const uint8_t minutes) {
  if (minutes == 0) {
    return 0;
  }
  return static_cast<unsigned long>(minutes) * 60UL * 1000UL;
}
}  // namespace

unsigned long Settings::lightSleepTimeoutMs() const { return minutesToMs(sleepTimeoutMinutes); }

unsigned long Settings::trueSleepTimeoutMs() const { return minutesToMs(trueSleepMinutes); }
