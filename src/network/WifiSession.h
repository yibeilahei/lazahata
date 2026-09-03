#pragma once

class Gfx;

// Lifetime of a Wi-Fi file-transfer visit. The ESP32-C3 has one DRAM heap:
// the reader's ~100 KB page buffer and Wi-Fi/mDNS cannot both be allocated
// after the framebuffer has split the heap, so this session borrows that
// buffer and restores it by restarting when the visit ends.
namespace WifiSession {

// Free the reader scratch buffers. Call once when entering the Wi-Fi flow.
void begin();

// Disconnect STA and reboot. setup() reserves the reader buffers from a
// clean heap and lands on Home. Does not return.
[[noreturn]] void end(Gfx& gfx);

}  // namespace WifiSession
