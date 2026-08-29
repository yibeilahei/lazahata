#include "core/SdUpdate.h"

#include <Gfx.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "core/fontIds.h"

namespace {
constexpr size_t kChunk = 4096;
constexpr size_t kMinBytes = 1024;

void show(Gfx& gfx, const char* line1, const char* line2 = nullptr) {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2 - 16, line1);
  if (line2 && line2[0]) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 16, line2);
  }
  gfx.present(HalDisplay::HALF_REFRESH);
}

size_t partitionLimit() {
  const esp_partition_t* dest = esp_ota_get_next_update_partition(nullptr);
  if (dest && dest->size > 0) {
    return dest->size;
  }
  return 0x640000;
}
}  // namespace

sdUpdate::Check sdUpdate::inspect(const char* path) {
  Check out;
  if (!path || path[0] == '\0' || !Storage.exists(path)) {
    out.error = "file not found";
    return out;
  }
  HalFile file;
  if (!Storage.openFileForRead("FW", path, file) || !file) {
    out.error = "could not open file";
    return out;
  }
  out.size = file.fileSize();
  uint8_t magic = 0;
  const int n0 = file.read(&magic, 1);
  file.close();
  if (n0 != 1 || magic != 0xE9) {
    out.error = "not an ESP32 image";
    return out;
  }
  if (out.size < kMinBytes) {
    out.error = "file too small";
    return out;
  }
  const size_t limit = partitionLimit();
  if (out.size > limit) {
    out.error = "file too large";
    return out;
  }
  out.ok = true;
  out.error = nullptr;
  return out;
}

bool sdUpdate::flash(const char* path, ProgressFn onProgress, void* ctx) {
  const Check check = inspect(path);
  if (!check.ok) {
    LOG_ERR("FW", "Reject %s: %s", path ? path : "", check.error ? check.error : "");
    return false;
  }

  HalFile file;
  if (!Storage.openFileForRead("FW", path, file) || !file) {
    LOG_ERR("FW", "Reopen failed %s", path);
    return false;
  }

  LOG_INF("FW", "Flashing %s (%u bytes)", path, static_cast<unsigned>(check.size));
  if (!Update.begin(check.size, U_FLASH)) {
    LOG_ERR("FW", "Update.begin failed: %s", Update.errorString());
    file.close();
    return false;
  }

  uint8_t chunk[kChunk];
  size_t written = 0;
  while (written < check.size) {
    const size_t want = (check.size - written > kChunk) ? kChunk : (check.size - written);
    const int n = file.read(chunk, want);
    if (n <= 0) {
      LOG_ERR("FW", "Short read at %u", static_cast<unsigned>(written));
      Update.abort();
      file.close();
      return false;
    }
    if (Update.write(chunk, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
      LOG_ERR("FW", "Write failed at %u: %s", static_cast<unsigned>(written), Update.errorString());
      Update.abort();
      file.close();
      return false;
    }
    written += static_cast<size_t>(n);
    if (onProgress) {
      onProgress(written, check.size, ctx);
    }
  }
  file.close();

  if (!Update.end(true)) {
    LOG_ERR("FW", "Update.end failed: %s", Update.errorString());
    return false;
  }
  LOG_INF("FW", "Flash ok");
  return true;
}

bool sdUpdate::tryApply(Gfx& gfx) {
  if (!Storage.exists(kPath)) {
    return false;
  }
  const Check check = inspect(kPath);
  if (!check.ok) {
    LOG_ERR("FW", "Reject %s: %s", kPath, check.error ? check.error : "");
    show(gfx, "Bad update.bin", check.error);
    return false;
  }
  show(gfx, "Updating firmware", "Keep the card in");
  if (!flash(kPath, nullptr, nullptr)) {
    show(gfx, "Update failed", Update.errorString());
    return false;
  }
  Storage.remove(kPath);
  show(gfx, "Update complete", "Restarting");
  delay(500);
  esp_restart();
  return true;
}
