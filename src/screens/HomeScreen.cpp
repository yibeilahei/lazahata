#include "HomeScreen.h"

#include <Gfx.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "core/Settings.h"
#include "core/UiList.h"
#include "core/fontIds.h"

void HomeScreen::refreshMenu() {
  const bool hasContinue = settings.lastBookPath[0] != '\0' && Storage.exists(settings.lastBookPath);
  itemCount = hasContinue ? 3 : 2;
  if (index >= itemCount) {
    index = 0;
  }
}

void HomeScreen::onEnter() {
  Screen::onEnter();
  refreshMenu();
  LOG_INF("HOME", "Continue %s last='%s'", itemCount == 3 ? "yes" : "no", settings.lastBookPath);
  requestUpdate();
}

void HomeScreen::onResume() {
  Screen::onResume();
  refreshMenu();
}

void HomeScreen::loop() {
  if (ui::applyDelta(index, input.consumeNavigationDelta(), itemCount)) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    const bool hasContinue = itemCount == 3;
    if (hasContinue && index == 0) {
      LOG_DBG("HOME", "Continue");
      goToReader(settings.lastBookPath);
    } else if ((hasContinue && index == 1) || (!hasContinue && index == 0)) {
      LOG_DBG("HOME", "Browse");
      goToBrowser();
    } else {
      LOG_DBG("HOME", "Settings");
      goToSettings();
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
    ui::drawMenuRow(gfx, startY + i * rowH, rowH, labels[i], i == index);
  }

  if (hasContinue) {
    gfx.drawText(FONT_UI, 24, gfx.height() - 48, settings.lastBookPath);
  }
  presentUi();
}
