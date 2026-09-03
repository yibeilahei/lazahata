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

// Short power-button press (released before the deep-sleep hold) toggles a
// gyroscope lock so picking up the device cannot turn pages. Any other key
// press clears the lock and is still handled normally. Idle timers: light
// sleep → the same lock; true sleep → white page and power off. A long
// power hold still deep-sleeps.
bool tiltLocked();
bool maybeToggleTiltLock(HalGPIO& gpio);
}  // namespace power
