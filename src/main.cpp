#include <Arduino.h>
#include <BoardConfig.h>
#include <Gfx.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <HalFrontlight.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <HalTiltSensor.h>
#include <Logging.h>
#include <WiFi.h>
#include <builtinFonts/all.h>

#include "ActivityManager.h"
#include "MappedInput.h"
#include "Settings.h"
#include "fontIds.h"

#ifndef XTCH_VERSION
#define XTCH_VERSION "dev"
#endif

Gfx gfx(display);
MappedInput mappedInput(gpio);
ActivityManager activityManager(gfx, mappedInput);

EpdFont ui12RegularFont(&ubuntu_12_medium);
EpdFont ui12BoldFont(&ubuntu_12_bold);
EpdFontFamily ui12Family(&ui12RegularFont, &ui12BoldFont);
EpdFontFamily ui12BoldFamily(&ui12BoldFont, &ui12BoldFont);

static unsigned long allowSleepAt = 0;
static bool wakePowerReleasePending = false;

void setup() {
#ifdef ENABLE_SERIAL_LOG
  logSerial.begin(115200);
#if LOG_SERIAL_HAS_TX_TIMEOUT
  logSerial.setTxTimeoutMs(1);
#endif
#endif

  WiFi.mode(WIFI_OFF);

  HalSystem::begin();
  gpio.begin();
  powerManager.begin();
  halTiltSensor.begin();
  halClock.begin();

  const auto wakeupReason = gpio.getWakeupReason();
  LOG_INF("MAIN", "CrossXTCH " XTCH_VERSION " device=%s", BoardConfig::ACTIVE.name);

  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD init failed");
    display.begin();
    gfx.begin();
    gfx.insertFont(FONT_UI, &ui12Family);
    gfx.insertFont(FONT_UI_BOLD, &ui12BoldFamily);
    activityManager.showMessage("SD card error");
    return;
  }

  HalSystem::checkPanic();
  settings.load();
  Frontlight.begin(0, 0, false);

  switch (wakeupReason) {
    case HalGPIO::WakeupReason::PowerButton:
      if (!gpio.verifyPowerButtonWakeup(800, true)) {
        powerManager.startDeepSleep(gpio);
      }
      wakePowerReleasePending = true;
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      powerManager.startDeepSleep(gpio);
      break;
    default:
      break;
  }

  display.begin();
  gfx.begin();
  gfx.insertFont(FONT_UI, &ui12Family);
  gfx.insertFont(FONT_UI_BOLD, &ui12BoldFamily);
  display.setInverted(settings.nightMode != 0);

  if (wakeupReason == HalGPIO::WakeupReason::PowerButton && settings.lastBookPath[0] != '\0' &&
      Storage.exists(settings.lastBookPath)) {
    activityManager.goToReader(settings.lastBookPath);
  } else {
    activityManager.goHome();
  }

  allowSleepAt = millis() + 2000;
}

void loop() {
  mappedInput.update();

  if (wakePowerReleasePending && !gpio.isPressed(HalGPIO::BTN_POWER)) {
    wakePowerReleasePending = false;
    return;
  }

  static unsigned long lastActivity = millis();
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased()) {
    lastActivity = millis();
    powerManager.setPowerSaving(false);
  }

  static bool powerReleasedSinceWake = false;
  if (!gpio.isPressed(HalGPIO::BTN_POWER)) {
    powerReleasedSinceWake = true;
  }
  if (powerReleasedSinceWake && millis() >= allowSleepAt && gpio.isPressed(HalGPIO::BTN_POWER) &&
      gpio.getPowerButtonHeldTime() > 800) {
    LOG_INF("SLP", "Power hold, sleeping");
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return;
  }

  const unsigned long sleepMs = settings.sleepTimeoutMs();
  if (sleepMs > 0 && millis() - lastActivity >= sleepMs) {
    LOG_INF("SLP", "Idle timeout");
    display.deepSleep();
    powerManager.startDeepSleep(gpio);
    return;
  }

  activityManager.loop();

  if (millis() - lastActivity >= HalPowerManager::IDLE_POWER_SAVING_MS) {
    powerManager.setPowerSaving(true);
    delay(50);
  } else {
    delay(10);
  }
}
