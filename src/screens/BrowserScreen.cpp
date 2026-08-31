#include "BrowserScreen.h"

#include <Gfx.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "core/UiList.h"
#include "core/fontIds.h"
#include "screens/UpdateScreen.h"

namespace {
bool hasExt(const char* name, const char* ext) {
  const size_t n = strlen(name);
  const size_t e = strlen(ext);
  if (n < e) {
    return false;
  }
  for (size_t i = 0; i < e; ++i) {
    char a = name[n - e + i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (a != ext[i]) {
      return false;
    }
  }
  return true;
}

void joinPath(char* out, size_t outSize, const char* dir, const char* name) {
  if (strcmp(dir, "/") == 0) {
    snprintf(out, outSize, "/%s", name);
  } else {
    snprintf(out, outSize, "%s/%s", dir, name);
  }
}
}  // namespace

BrowserScreen::BrowserScreen(Gfx& gfx, MappedInput& input, const char* initialPath, const Mode mode)
    : Screen(mode == Mode::Firmware ? "Firmware" : "Browser", gfx, input), mode(mode) {
  snprintf(path, sizeof(path), "%s", initialPath && initialPath[0] ? initialPath : "/");
}

void BrowserScreen::load() {
  entries.clear();
  entries.reserve(64);
  HalFile root = Storage.open(path);
  if (!root || !root.isDirectory()) {
    LOG_ERR("DIR", "Cannot open %s", path);
    return;
  }
  char name[128];
  for (HalFile file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      continue;
    }
    if (file.isDirectory()) {
      std::string row = name;
      row += '/';
      entries.push_back(std::move(row));
    } else if (mode == Mode::Firmware ? hasExt(name, ".bin") : hasExt(name, ".xtch")) {
      entries.emplace_back(name);
    }
  }
  std::sort(entries.begin(), entries.end());
  if (index >= static_cast<int>(entries.size())) {
    index = 0;
  }
  window = 0;
  LOG_INF("DIR", "%s (%u items)", path, static_cast<unsigned>(entries.size()));
}

void BrowserScreen::onEnter() {
  Screen::onEnter();
  load();
  requestUpdate();
}

void BrowserScreen::goUp() {
  if (strcmp(path, "/") == 0) {
    finish();
    return;
  }
  char* slash = strrchr(path, '/');
  if (!slash || slash == path) {
    snprintf(path, sizeof(path), "/");
  } else {
    *slash = '\0';
  }
  index = 0;
  load();
  requestUpdate();
}

void BrowserScreen::activate() {
  if (entries.empty()) {
    return;
  }
  const std::string& name = entries[static_cast<size_t>(index)];
  char next[256];
  if (!name.empty() && name.back() == '/') {
    std::string dir = name.substr(0, name.size() - 1);
    joinPath(next, sizeof(next), path, dir.c_str());
    snprintf(path, sizeof(path), "%s", next);
    LOG_DBG("DIR", "Enter %s", path);
    index = 0;
    load();
    requestUpdate();
    return;
  }
  joinPath(next, sizeof(next), path, name.c_str());
  LOG_INF("DIR", "Open %s", next);
  if (mode == Mode::Firmware) {
    auto screen = makeUniqueNoThrow<UpdateScreen>(gfx, input, next);
    if (!screen) {
      LOG_ERR("DIR", "OOM: update");
      return;
    }
    push(std::move(screen));
    return;
  }
  goToReader(next);
}

void BrowserScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    goUp();
    return;
  }
  const int count = static_cast<int>(entries.size());
  if (ui::applyDelta(index, input.consumeNavigationDelta(), count)) {
    requestUpdate();
  } else if (count > 0 && input.wasReleased(MappedInput::Button::Confirm)) {
    activate();
  }
}

void BrowserScreen::render() {
  gfx.clear(false);
  gfx.drawText(FONT_UI_BOLD, 12, 8, path);

  const int rowH = gfx.lineHeight(FONT_UI) + 8;
  const int top = 40;
  const int bottomPad = mode == Mode::Firmware ? 52 : 24;
  const int rows = (gfx.height() - top - bottomPad) / rowH;
  ui::followWindow(window, index, rows);
  if (entries.empty()) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2, mode == Mode::Firmware ? "No .bin files" : "No books");
  } else {
    const int last = std::min(window + rows, static_cast<int>(entries.size()));
    for (int i = window; i < last; ++i) {
      ui::drawRow(gfx, top + (i - window) * rowH, rowH, entries[static_cast<size_t>(i)].c_str(), i == index);
    }
  }
  if (mode == Mode::Firmware) {
    gfx.drawCenteredText(FONT_UI, gfx.height() - 28, "Back to cancel");
  }
  presentUi();
}
