#include "SettingsScreen.h"

#include <Gfx.h>
#include <HalDisplay.h>
#include <HalTiltSensor.h>
#include <Logging.h>

#include <cstdio>

#include "core/Settings.h"
#include "core/UiList.h"
#include "core/fontIds.h"

#ifndef LAZAHATA_VERSION
#define LAZAHATA_VERSION "dev"
#endif

namespace {
// Tilt page-turn is only offered on boards with the QMI8658 IMU (X3).
bool hasTilt() { return halTiltSensor.isAvailable(); }
int itemCount() { return hasTilt() ? 6 : 5; }
int tiltIndex() { return 3; }
int firmwareIndex() { return hasTilt() ? 4 : 3; }
int backIndex() { return hasTilt() ? 5 : 4; }

const char* sleepLabel() {
  switch (settings.sleepTimeoutMinutes) {
    case 0:
      return "Sleep: never";
    case 3:
      return "Sleep: 3 min";
    case 10:
      return "Sleep: 10 min";
    default:
      return "Sleep: 5 min";
  }
}

const char* refreshLabel() {
  switch (settings.refreshEveryNPages) {
    case 1:
      return "Refresh: every page";
    case 10:
      return "Refresh: every 10 pages";
    case 15:
      return "Refresh: every 15 pages";
    case 20:
      return "Refresh: every 20 pages";
    default:
      return "Refresh: every 5 pages";
  }
}

void bumpSleep() {
  switch (settings.sleepTimeoutMinutes) {
    case 0:
      settings.sleepTimeoutMinutes = 3;
      break;
    case 3:
      settings.sleepTimeoutMinutes = 5;
      break;
    case 5:
      settings.sleepTimeoutMinutes = 10;
      break;
    default:
      settings.sleepTimeoutMinutes = 0;
      break;
  }
}

void bumpRefresh() {
  switch (settings.refreshEveryNPages) {
    case 1:
      settings.refreshEveryNPages = 5;
      break;
    case 5:
      settings.refreshEveryNPages = 10;
      break;
    case 10:
      settings.refreshEveryNPages = 15;
      break;
    case 15:
      settings.refreshEveryNPages = 20;
      break;
    default:
      settings.refreshEveryNPages = 1;
      break;
  }
}
}  // namespace

void SettingsScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    settings.save();
    finish();
    return;
  }
  if (ui::applyDelta(index, input.consumeNavigationDelta(), itemCount())) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    if (index == 0) {
      bumpSleep();
      LOG_INF("SET", "%s", sleepLabel());
    } else if (index == 1) {
      bumpRefresh();
      LOG_INF("SET", "%s", refreshLabel());
    } else if (index == 2) {
      settings.nightMode = settings.nightMode ? 0 : 1;
      display.setInverted(settings.nightMode != 0);
      LOG_INF("SET", "Night mode %s", settings.nightMode ? "on" : "off");
    } else if (hasTilt() && index == tiltIndex()) {
      settings.tiltPageTurn = settings.tiltPageTurn ? 0 : 1;
      LOG_INF("SET", "Tilt page turn %s", settings.tiltPageTurn ? "on" : "off");
    } else if (index == firmwareIndex()) {
      settings.save();
      goToFirmwareUpdate();
      return;
    } else {
      settings.save();
      finish();
      return;
    }
    settings.save();
    requestUpdate();
  }
}

void SettingsScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 24, "Settings");

  char night[32];
  snprintf(night, sizeof(night), "Night mode: %s", settings.nightMode ? "on" : "off");
  char tilt[32];
  snprintf(tilt, sizeof(tilt), "Tilt page turn: %s", settings.tiltPageTurn ? "on" : "off");

  const char* labels[6];
  labels[0] = sleepLabel();
  labels[1] = refreshLabel();
  labels[2] = night;
  if (hasTilt()) {
    labels[3] = tilt;
    labels[4] = "Update firmware";
    labels[5] = "Back";
  } else {
    labels[3] = "Update firmware";
    labels[4] = "Back";
  }

  const int rowH = gfx.lineHeight(FONT_UI) + 10;
  const int startY = 90;
  for (int i = 0; i < itemCount(); ++i) {
    ui::drawMenuRow(gfx, startY + i * rowH, rowH, labels[i], i == index);
  }

  gfx.drawCenteredText(FONT_UI, gfx.height() - 40, "lazahata " LAZAHATA_VERSION);
  presentUi();
}
