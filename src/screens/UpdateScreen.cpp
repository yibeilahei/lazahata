#include "UpdateScreen.h"

#include <Gfx.h>
#include <HalDisplay.h>
#include <Logging.h>
#include <Update.h>
#include <esp_system.h>

#include <cstdio>
#include <cstring>

#include "core/SdLog.h"
#include "core/fontIds.h"

namespace {
const char* fileName(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

void progressCb(size_t written, size_t total, void* ctx) {
  static_cast<UpdateScreen*>(ctx)->drawProgress(written, total);
}
}  // namespace

void UpdateScreen::drawProgress(const size_t written, const size_t total) {
  const int pct = total > 0 ? static_cast<int>((written * 100) / total) : 0;
  if (pct / 5 == lastPercent / 5 && pct != 100 && lastPercent >= 0) {
    return;
  }
  lastPercent = pct;
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2 - 40, "Updating firmware");
  char line[16];
  snprintf(line, sizeof(line), "%d%%", pct);
  gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 8, line);
  const int barX = 24;
  const int barW = gfx.width() - 48;
  const int barY = gfx.height() / 2 + 20;
  gfx.fillRect(barX, barY, barW, 18, true);
  const int fill = (barW - 4) * pct / 100;
  if (fill > 0) {
    gfx.fillRect(barX + 2, barY + 2, fill, 14, false);
  }
  gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 52, "Do not power off");
  gfx.present(HalDisplay::FAST_REFRESH);
}

UpdateScreen::UpdateScreen(Gfx& gfx, MappedInput& input, const char* path) : Activity("Update", gfx, input) {
  snprintf(firmwarePath, sizeof(firmwarePath), "%s", path ? path : "");
}

void UpdateScreen::onEnter() {
  Activity::onEnter();
  check = sdUpdate::inspect(firmwarePath);
  if (!check.ok) {
    error = check.error ? check.error : "invalid firmware";
    state = State::Failed;
    LOG_ERR("FW", "Inspect %s: %s", firmwarePath, error);
  }
  requestUpdate();
}

void UpdateScreen::loop() {
  if (state == State::Confirm) {
    if (input.wasReleased(MappedInput::Button::Back)) {
      finish();
      return;
    }
    if (input.wasReleased(MappedInput::Button::Confirm)) {
      state = State::Updating;
      requestUpdate();
      return;
    }
  } else if (state == State::Updating && !started) {
    started = true;
    sdlog::flush();
    drawProgress(0, check.size);
    if (!sdUpdate::flash(firmwarePath, progressCb, this)) {
      error = Update.errorString();
      if (!error || error[0] == '\0') {
        error = "write failed";
      }
      state = State::Failed;
      requestUpdate();
      return;
    }
    sdlog::flush();
    gfx.clear(false);
    gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2, "Update complete");
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 28, "Restarting");
    gfx.present(HalDisplay::HALF_REFRESH);
    delay(800);
    esp_restart();
  } else if (state == State::Failed) {
    if (input.wasReleased(MappedInput::Button::Back) || input.wasReleased(MappedInput::Button::Confirm)) {
      finish();
    }
  }
}

void UpdateScreen::render() {
  if (state == State::Updating) {
    return;
  }
  gfx.clear(false);
  if (state == State::Failed) {
    gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2 - 16, "Update failed");
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 16, error ? error : "");
  } else {
    gfx.drawCenteredText(FONT_UI_BOLD, 36, "Update firmware?");
    gfx.drawCenteredText(FONT_UI, 100, fileName(firmwarePath));
    char sizeLine[32];
    snprintf(sizeLine, sizeof(sizeLine), "%u KB", static_cast<unsigned>(check.size / 1024));
    gfx.drawCenteredText(FONT_UI, 130, sizeLine);
    gfx.drawCenteredText(FONT_UI, gfx.height() - 80, "Confirm to flash");
    gfx.drawCenteredText(FONT_UI, gfx.height() - 52, "Back to cancel");
  }
  gfx.present(HalDisplay::FAST_REFRESH);
}
