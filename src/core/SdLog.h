#pragma once

// Appends INF/ERR to /lazahata.log. Lines are RAM-buffered and flushed every
// few seconds, when the buffer fills, or before sleep — not on each log.
namespace sdlog {
constexpr const char* kPath = "/lazahata.log";

void attach();
void begin();
void flush();
void poll();
}  // namespace sdlog
