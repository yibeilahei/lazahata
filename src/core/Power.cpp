#include "core/Power.h"

#include <Arduino.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <Logging.h>

#include "core/ScreenManager.h"
#include "core/Settings.h"

namespace {
unsigned long allowSleepAt = 0;
unsigned long lastActivity = 0;
bool wakePowerReleasePending = false;
bool powerReleasedSinceWake = false;
}  // namespace

void power::noteWakeHold() {
  wakePowerReleasePending = true;
  allowSleepAt = millis() + 2000;
  lastActivity = millis();
  LOG_DBG("SLP", "Wake hold, sleep allowed in 2s");
}

bool power::consumeWakeRelease(HalGPIO& gpio) {
  if (wakePowerReleasePending && !gpio.isPressed(HalGPIO::BTN_POWER)) {
    wakePowerReleasePending = false;
    return true;
  }
  return false;
}

void power::noteUserActivity(HalGPIO& gpio) {
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased()) {
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
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return true;
  }

  const unsigned long sleepMs = settings.sleepTimeoutMs();
  if (sleepMs > 0 && millis() - lastActivity >= sleepMs) {
    LOG_INF("SLP", "Idle timeout");
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return true;
  }
  return false;
}

void power::idleDelay() {
  if (millis() - lastActivity >= HalPowerManager::IDLE_POWER_SAVING_MS) {
    powerManager.setPowerSaving(true);
    delay(50);
  } else {
    delay(10);
  }
}
