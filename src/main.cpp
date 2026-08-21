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
#include "core/SdLog.h"
#include "core/SdUpdate.h"
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

static const char* wakeupName(HalGPIO::WakeupReason reason) {
  switch (reason) {
    case HalGPIO::WakeupReason::PowerButton:
      return "power";
    case HalGPIO::WakeupReason::AfterFlash:
      return "flash";
    case HalGPIO::WakeupReason::AfterUSBPower:
      return "usb";
    default:
      return "other";
  }
}

static void setupDisplayAndFonts() {
  display.begin();
  gfx.begin();
  gfx.insertFont(FONT_UI, &ui12Family);
  gfx.insertFont(FONT_UI_BOLD, &ui12BoldFamily);
}

void setup() {
  sdlog::attach();
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
  LOG_INF("MAIN", "Lazahata " LAZAHATA_VERSION " board=%s gpio=%s wake=%s bat=%u%% panic=%d",
          BoardConfig::ACTIVE.name, gpio.deviceIsX3() ? "x3" : "not-x3", wakeupName(wakeupReason),
          static_cast<unsigned>(powerManager.getBatteryPercentage()), HalSystem::isRebootFromPanic() ? 1 : 0);

  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD init failed");
    setupDisplayAndFonts();
    activityManager.showMessage("SD card error");
    return;
  }
  sdlog::begin();
  LOG_INF("MAIN", "SD ready, log=%s", sdlog::kPath);

  HalSystem::checkPanic();
  settings.load();
  Frontlight.begin(0, 0, false);

  bool recoveryFirmware = false;
  switch (wakeupReason) {
    case HalGPIO::WakeupReason::PowerButton:
      if (!gpio.verifyPowerButtonWakeup(800, true)) {
        LOG_INF("MAIN", "Power press too short, sleeping");
        sdlog::flush();
        powerManager.startDeepSleep(gpio);
      } else {
        LOG_INF("MAIN", "Power wake, waiting for button release");
        power::noteWakeHold();
        const unsigned long settleStart = millis();
        while (millis() - settleStart < 500) {
          gpio.update();
          delay(10);
        }
        if (gpio.isPressed(HalGPIO::BTN_UP)) {
          recoveryFirmware = true;
          LOG_INF("MAIN", "Recovery firmware mode (UP + POWER)");
        }
      }
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      LOG_INF("MAIN", "USB-power wake, sleeping");
      sdlog::flush();
      powerManager.startDeepSleep(gpio);
      break;
    default:
      break;
  }

  setupDisplayAndFonts();
  display.setInverted(settings.nightMode != 0);
  LOG_INF("MAIN", "Display up night=%u", settings.nightMode);

  if (recoveryFirmware) {
    activityManager.goToFirmwareUpdate(true);
    return;
  }

  if (sdUpdate::tryApply(gfx)) {
    return;
  }

  if (wakeupReason == HalGPIO::WakeupReason::PowerButton && settings.lastBookPath[0] != '\0' &&
      Storage.exists(settings.lastBookPath)) {
    LOG_INF("MAIN", "Resume %s", settings.lastBookPath);
    activityManager.goToReader(settings.lastBookPath);
  } else {
    LOG_INF("MAIN", "Go home (last='%s')", settings.lastBookPath);
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
  sdlog::poll();
  power::idleDelay();
}
