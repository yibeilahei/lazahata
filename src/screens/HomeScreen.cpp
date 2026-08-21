#include "HomeScreen.h"

#include <Gfx.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "core/ActivityManager.h"
#include "core/Settings.h"
#include "core/fontIds.h"

void HomeScreen::onEnter() {
  Activity::onEnter();
  const bool hasContinue = settings.lastBookPath[0] != '\0' && Storage.exists(settings.lastBookPath);
  itemCount = hasContinue ? 3 : 2;
  if (index >= itemCount) {
    index = 0;
  }
  LOG_INF("HOME", "Continue %s last='%s'", hasContinue ? "yes" : "no", settings.lastBookPath);
  requestUpdate();
}

void HomeScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Up) || input.wasReleased(MappedInput::Button::Left)) {
    index = (index + itemCount - 1) % itemCount;
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Down) || input.wasReleased(MappedInput::Button::Right)) {
    index = (index + 1) % itemCount;
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    const bool hasContinue = itemCount == 3;
    if (hasContinue && index == 0) {
      LOG_DBG("HOME", "Continue");
      goToReader(settings.lastBookPath);
    } else if ((hasContinue && index == 1) || (!hasContinue && index == 0)) {
      LOG_DBG("HOME", "Browse");
      activityManager.goToBrowser();
    } else {
      LOG_DBG("HOME", "Settings");
      activityManager.goToSettings();
    }
  }
}

void HomeScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 36, "Lazahata");

  char bat[16];
  snprintf(bat, sizeof(bat), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  gfx.drawText(FONT_UI, gfx.width() - gfx.textWidth(FONT_UI, bat) - 12, 8, bat);

  const bool hasContinue = itemCount == 3;
  const char* labels[3];
  int n = 0;
  if (hasContinue) {
    labels[n++] = "Continue";
  }
  labels[n++] = "Browse";
  labels[n++] = "Settings";

  const int rowH = gfx.lineHeight(FONT_UI) + 10;
  const int startY = 120;
  for (int i = 0; i < n; ++i) {
    const int y = startY + i * rowH;
    if (i == index) {
      gfx.fillRect(24, y - 4, gfx.width() - 48, rowH, true);
      gfx.drawText(FONT_UI, 40, y, labels[i], false);
    } else {
      gfx.drawText(FONT_UI, 40, y, labels[i], true);
    }
  }

  if (hasContinue) {
    gfx.drawText(FONT_UI, 24, gfx.height() - 48, settings.lastBookPath);
  }
  gfx.present(HalDisplay::HALF_REFRESH);
}
