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

#include "core/ActivityManager.h"
#include "core/MappedInput.h"
#include "core/Power.h"
#include "core/Settings.h"
#include "core/fontIds.h"

#ifndef LAZAHATA_VERSION
#define LAZAHATA_VERSION "dev"
#endif

Gfx gfx(display);
MappedInput mappedInput(gpio);
ActivityManager activityManager(gfx, mappedInput);

EpdFont ui12RegularFont(&ubuntu_12_medium);
EpdFont ui12BoldFont(&ubuntu_12_bold);
EpdFontFamily ui12Family(&ui12RegularFont, &ui12BoldFont);
EpdFontFamily ui12BoldFamily(&ui12BoldFont, &ui12BoldFont);

static void setupDisplayAndFonts() {
  display.begin();
  gfx.begin();
  gfx.insertFont(FONT_UI, &ui12Family);
  gfx.insertFont(FONT_UI_BOLD, &ui12BoldFamily);
}

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
  LOG_INF("MAIN", "Lazahata " LAZAHATA_VERSION " device=%s", BoardConfig::ACTIVE.name);

  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD init failed");
    setupDisplayAndFonts();
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
      power::noteWakeHold();
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      powerManager.startDeepSleep(gpio);
      break;
    default:
      break;
  }

  setupDisplayAndFonts();
  display.setInverted(settings.nightMode != 0);

  if (wakeupReason == HalGPIO::WakeupReason::PowerButton && settings.lastBookPath[0] != '\0' &&
      Storage.exists(settings.lastBookPath)) {
    activityManager.goToReader(settings.lastBookPath);
  } else {
    activityManager.goHome();
  }
}

void loop() {
  mappedInput.update();
  if (power::consumeWakeRelease(gpio)) {
    return;
  }
  power::noteUserActivity(gpio);
  if (power::maybeSleep(gpio, settings)) {
    return;
  }
  activityManager.loop();
  power::idleDelay();
}
