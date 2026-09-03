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

// Light sleep: a short power-button press (released before the deep-sleep
// hold threshold) puts the device into a state where it stays powered on but
// ignores all button/touch/tilt input. Any physical button press wakes it
// back up (tilt/flick gestures do not). Unlike maybeSleep()'s deep sleep,
// this does not power the board down or reset it on wake.
bool isAsleep();
// Call once per loop iteration (after mappedInput.update()). Returns true if
// this call just toggled light sleep on or off, in which case the caller
// should skip the rest of the iteration.
bool maybeToggleLightSleep(HalGPIO& gpio);
}  // namespace power
