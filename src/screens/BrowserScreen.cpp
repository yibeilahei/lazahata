#include "BrowserScreen.h"

#include <Gfx.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "ActivityManager.h"
#include "fontIds.h"

namespace {
bool hasXtchExt(const char* name) {
  const size_t n = strlen(name);
  constexpr char ext[] = ".xtch";
  constexpr size_t e = sizeof(ext) - 1;
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

BrowserScreen::BrowserScreen(Gfx& gfx, MappedInput& input, const char* initialPath) : Activity("Browser", gfx, input) {
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
    } else if (hasXtchExt(name)) {
      entries.emplace_back(name);
    }
  }
  std::sort(entries.begin(), entries.end());
  if (index >= static_cast<int>(entries.size())) {
    index = 0;
  }
  window = 0;
}

void BrowserScreen::onEnter() {
  Activity::onEnter();
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
    index = 0;
    load();
    requestUpdate();
    return;
  }
  joinPath(next, sizeof(next), path, name.c_str());
  goToReader(next);
}

void BrowserScreen::loop() {
  const int count = static_cast<int>(entries.size());
  if (input.wasReleased(MappedInput::Button::Back)) {
    goUp();
    return;
  }
  if (count == 0) {
    return;
  }
  if (input.wasReleased(MappedInput::Button::Up) || input.wasReleased(MappedInput::Button::Left)) {
    index = (index + count - 1) % count;
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Down) || input.wasReleased(MappedInput::Button::Right)) {
    index = (index + 1) % count;
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    activate();
  }
}

void BrowserScreen::render() {
  gfx.clear(false);
  gfx.drawText(FONT_UI_BOLD, 12, 8, path);

  const int rowH = gfx.lineHeight(FONT_UI) + 8;
  const int top = 40;
  const int rows = (gfx.height() - top - 24) / rowH;
  if (index < window) {
    window = index;
  }
  if (index >= window + rows) {
    window = index - rows + 1;
  }
  if (entries.empty()) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2, "No books");
  } else {
    const int last = std::min(window + rows, static_cast<int>(entries.size()));
    for (int i = window; i < last; ++i) {
      const int y = top + (i - window) * rowH;
      const char* label = entries[static_cast<size_t>(i)].c_str();
      if (i == index) {
        gfx.fillRect(8, y - 2, gfx.width() - 16, rowH, true);
        gfx.drawText(FONT_UI, 16, y, label, false);
      } else {
        gfx.drawText(FONT_UI, 16, y, label, true);
      }
    }
  }
  gfx.present(HalDisplay::FAST_REFRESH);
}
