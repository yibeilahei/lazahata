#include "core/Power.h"

#include <Arduino.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalTiltSensor.h>
#include <Logging.h>

#include "core/ScreenManager.h"
#include "core/Settings.h"

namespace {
unsigned long allowSleepAt = 0;
unsigned long lastActivity = 0;
bool wakePowerReleasePending = false;
bool powerReleasedSinceWake = false;
bool lightAsleep = false;
// Set when light sleep was just woken by a power-button press, so that
// button's own release (still part of the same physical press) isn't also
// read as the short press that re-enters light sleep.
bool suppressNextPowerShortPress = false;
// Release before this held duration counts as a short press (toggles light
// sleep); at or beyond it, the button is treated as the deep-sleep hold in
// maybeSleep() instead.
constexpr unsigned long kShortPressMs = 800;
}  // namespace

void power::noteWakeHold() {
  wakePowerReleasePending = true;
  allowSleepAt = millis() + 2000;
  lastActivity = millis();
  LOG_DBG("SLP", "Wake hold, sleep allowed in 2s");
}

bool power::isWakeReleasePending() { return wakePowerReleasePending; }

bool power::consumeWakeRelease(HalGPIO& gpio) {
  if (wakePowerReleasePending && !gpio.isPressed(HalGPIO::BTN_POWER)) {
    wakePowerReleasePending = false;
    return true;
  }
  return false;
}

void power::noteUserActivity(HalGPIO& gpio) {
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || halTiltSensor.hadActivity()) {
    lastActivity = millis();
    powerManager.setPowerSaving(false);
  }
  if (!gpio.isPressed(HalGPIO::BTN_POWER)) {
    powerReleasedSinceWake = true;
  }
}

bool power::maybeSleep(HalGPIO& gpio, const Settings& settings) {
  if (screenManager.blocksSleep()) {
    return false;
  }
  if (lastActivity == 0) {
    lastActivity = millis();
  }
  if (powerReleasedSinceWake && millis() >= allowSleepAt && gpio.isPressed(HalGPIO::BTN_POWER) &&
      gpio.getPowerButtonHeldTime() > 800) {
    LOG_INF("SLP", "Power hold, sleeping");
    halTiltSensor.deepSleep();
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return true;
  }

  const unsigned long sleepMs = settings.sleepTimeoutMs();
  if (sleepMs > 0 && millis() - lastActivity >= sleepMs) {
    LOG_INF("SLP", "Idle timeout");
    halTiltSensor.deepSleep();
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return true;
  }
  return false;
}

bool power::isAsleep() { return lightAsleep; }

bool power::maybeToggleLightSleep(HalGPIO& gpio) {
  // The wake-hold release (finger still down right after waking from deep
  // sleep) is handled by consumeWakeRelease() and must not also toggle light
  // sleep.
  if (wakePowerReleasePending) {
    return false;
  }

  if (lightAsleep) {
    // Any physical button wakes it (tilt/flick gestures do not).
    if (gpio.wasAnyPressed()) {
      lightAsleep = false;
      // Don't let the time spent asleep count against the idle-to-deep-sleep
      // timeout.
      lastActivity = millis();
      // If the power button itself is what woke it, its upcoming release
      // shouldn't be read as the short press that re-enters light sleep.
      suppressNextPowerShortPress = gpio.wasPressed(HalGPIO::BTN_POWER);
      LOG_INF("SLP", "Button press, waking from light sleep");
      return true;
    }
    return false;
  }

  if (suppressNextPowerShortPress) {
    if (gpio.wasReleased(HalGPIO::BTN_POWER)) {
      suppressNextPowerShortPress = false;
    }
    return false;
  }

  if (gpio.wasReleased(HalGPIO::BTN_POWER) && gpio.getPowerButtonHeldTime() <= kShortPressMs) {
    lightAsleep = true;
    LOG_INF("SLP", "Short press, light sleep");
    return true;
  }
  return false;
}

void power::idleDelay() {
  // The idle power-saving delay only matters on the reader screen, where the
  // device typically sits for long stretches between button presses. Other
  // screens (home, settings, browser, file transfer) are short, active
  // interactions and shouldn't be throttled — the overall sleep timeout in
  // maybeSleep() still applies regardless.
  if (!screenManager.isReader()) {
    delay(1);
    return;
  }
  if (millis() - lastActivity >= HalPowerManager::IDLE_POWER_SAVING_MS) {
    powerManager.setPowerSaving(true);
    delay(50);
  } else {
    delay(10);
  }
}
