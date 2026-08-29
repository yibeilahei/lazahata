#pragma once

class HalGPIO;
struct Settings;

// Wake, idle timeout, and power-button hold-to-sleep. Owned by the main loop,
// not by individual screens.
namespace power {
void noteWakeHold();
bool isWakeReleasePending();
bool consumeWakeRelease(HalGPIO& gpio);
void noteUserActivity(HalGPIO& gpio);
bool maybeSleep(HalGPIO& gpio, const Settings& settings);
void idleDelay();
}  // namespace power
