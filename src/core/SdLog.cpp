#include "core/SdLog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace {
constexpr size_t kBufSize = 4096;
constexpr size_t kMaxFileBytes = 256 * 1024;
constexpr unsigned long kPollMs = 5000;

char buf[kBufSize];
size_t used = 0;
bool ready = false;
bool flushing = false;
unsigned long lastFlushMs = 0;

void appendLine(const char* line, bool /*urgent*/) {
  if (!line || line[0] == '\0') {
    return;
  }
  const size_t n = strlen(line);
  if (n >= kBufSize) {
    return;
  }
  if (used + n > kBufSize) {
    if (ready) {
      sdlog::flush();
    }
    if (used + n > kBufSize) {
      return;
    }
  }
  memcpy(buf + used, line, n);
  used += n;
}
}  // namespace

void sdlog::attach() { setLogSink(appendLine); }

void sdlog::begin() {
  ready = true;
  if (Storage.exists(kPath)) {
    HalFile existing = Storage.open(kPath, O_RDONLY);
    if (existing && existing.fileSize() > kMaxFileBytes) {
      existing.close();
      Storage.remove(kPath);
    }
  }
  flush();
}

void sdlog::flush() {
  if (!ready || used == 0 || flushing) {
    return;
  }
  flushing = true;
  HalFile file = Storage.open(kPath, O_WRITE | O_CREAT | O_APPEND);
  if (file) {
    const size_t n = file.write(buf, used);
    file.flush();
    file.close();
    if (n == used) {
      used = 0;
      lastFlushMs = millis();
    }
  }
  flushing = false;
}

void sdlog::poll() {
  if (ready && used > 0 && millis() - lastFlushMs >= kPollMs) {
    flush();
  }
}
