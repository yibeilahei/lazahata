#include "ReaderScreen.h"

#include <Gfx.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "Settings.h"
#include "fontIds.h"

namespace {
uint32_t pathHash(const char* path) {
  uint32_t h = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p) {
    h ^= *p;
    h *= 16777619u;
  }
  return h;
}

void progressPath(char* out, size_t outSize, const char* bookPath) {
  snprintf(out, outSize, "%s/p_%08lx.bin", Settings::kDir, static_cast<unsigned long>(pathHash(bookPath)));
}
}  // namespace

ReaderScreen::ReaderScreen(Gfx& gfx, MappedInput& input, const char* path) : Activity("Reader", gfx, input) {
  snprintf(bookPath, sizeof(bookPath), "%s", path ? path : "");
}

void ReaderScreen::loadProgress() {
  char p[64];
  progressPath(p, sizeof(p), bookPath);
  HalFile f;
  if (!Storage.openFileForRead("PRG", p, f)) {
    page = 0;
    return;
  }
  uint32_t saved = 0;
  if (f.read(&saved, sizeof(saved)) == static_cast<int>(sizeof(saved))) {
    page = saved;
  }
}

void ReaderScreen::saveProgress() const {
  Storage.ensureDirectoryExists(Settings::kDir);
  char p[64];
  progressPath(p, sizeof(p), bookPath);
  HalFile f;
  if (!Storage.openFileForWrite("PRG", p, f)) {
    LOG_ERR("PRG", "Could not write %s", p);
    return;
  }
  f.write(&page, sizeof(page));
}

void ReaderScreen::onEnter() {
  Activity::onEnter();
  pagesUntilFull = settings.refreshEveryNPages;
  if (book.open(bookPath) != xtc::Error::Ok) {
    LOG_ERR("RDR", "Failed to open %s: %s", bookPath, xtc::errorName(book.lastError()));
    loaded = false;
    requestUpdate();
    return;
  }
  loaded = true;
  loadProgress();
  if (page >= book.pageCount()) {
    page = book.pageCount() > 0 ? book.pageCount() - 1 : 0;
  }
  snprintf(settings.lastBookPath, sizeof(settings.lastBookPath), "%s", bookPath);
  settings.lastBookPage = page;
  settings.save();
  requestUpdate();
}

void ReaderScreen::onExit() {
  if (loaded) {
    saveProgress();
  }
  book.close();
  Activity::onExit();
}

void ReaderScreen::loop() {
  if (!loaded) {
    if (input.wasReleased(MappedInput::Button::Back) || input.wasReleased(MappedInput::Button::Confirm)) {
      finish();
    }
    return;
  }

  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }
  if (input.wasReleased(MappedInput::Button::Confirm)) {
    overlay = !overlay;
    requestUpdate();
    return;
  }

  bool moved = false;
  if (input.wasReleased(MappedInput::Button::PageForward) || input.wasReleased(MappedInput::Button::Right) ||
      input.wasReleased(MappedInput::Button::Down)) {
    if (page + 1 < book.pageCount()) {
      ++page;
      moved = true;
    }
  } else if (input.wasReleased(MappedInput::Button::PageBack) || input.wasReleased(MappedInput::Button::Left) ||
             input.wasReleased(MappedInput::Button::Up)) {
    if (page > 0) {
      --page;
      moved = true;
    }
  }
  if (moved) {
    settings.lastBookPage = page;
    saveProgress();
    requestUpdate();
  }
}

void ReaderScreen::render() {
  if (!loaded) {
    gfx.clear(false);
    gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2 - 20, "Could not open book");
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 10, xtc::errorName(book.lastError()));
    gfx.present(HalDisplay::HALF_REFRESH);
    return;
  }

  if (!book.drawPage(gfx, page)) {
    gfx.clear(false);
    gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2, xtc::errorName(book.lastError()));
    gfx.present(HalDisplay::HALF_REFRESH);
    return;
  }

  if (overlay) {
    char line[48];
    snprintf(line, sizeof(line), "%lu / %u", static_cast<unsigned long>(page + 1), book.pageCount());
    const int y = gfx.height() - gfx.lineHeight(FONT_UI) - 8;
    gfx.fillRect(0, y - 4, gfx.width(), gfx.lineHeight(FONT_UI) + 12, false);
    gfx.drawText(FONT_UI, 12, y, book.title());
    const int w = gfx.textWidth(FONT_UI, line);
    gfx.drawText(FONT_UI, gfx.width() - w - 12, y, line);
  }

  HalDisplay::RefreshMode mode = HalDisplay::FAST_REFRESH;
  if (pagesUntilFull <= 1) {
    mode = HalDisplay::HALF_REFRESH;
    pagesUntilFull = settings.refreshEveryNPages;
  } else {
    --pagesUntilFull;
  }
  gfx.present(mode);
}
