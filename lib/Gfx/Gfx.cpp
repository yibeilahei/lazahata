#include "Gfx.h"

#include <Logging.h>
#include <Utf8.h>

#include <cassert>
#include <cstring>

Gfx::Gfx(HalDisplay& display) : panel(display) {}

void Gfx::begin() {
  fb = panel.getFrameBuffer();
  assert(fb != nullptr && "framebuffer missing");
  panelWidth = panel.getDisplayWidth();
  panelHeight = panel.getDisplayHeight();
  panelWidthBytes = panel.getDisplayWidthBytes();
  // Panel is landscape; logical coordinates are portrait.
  logicalWidth = panelHeight;
  logicalHeight = panelWidth;
  LOG_INF("GFX", "Panel %ux%u, logical %dx%d", panelWidth, panelHeight, logicalWidth, logicalHeight);
}

void Gfx::insertFont(const int id, const EpdFontFamily* family) {
  for (auto& slot : fonts) {
    if (slot.family == nullptr || slot.id == id) {
      slot.id = id;
      slot.family = family;
      return;
    }
  }
  LOG_ERR("GFX", "Font table full, dropped id %d", id);
}

const EpdFontFamily* Gfx::font(const int id) const {
  for (const auto& slot : fonts) {
    if (slot.family && slot.id == id) {
      return slot.family;
    }
  }
  return nullptr;
}

void Gfx::toPanel(const int x, const int y, int& phyX, int& phyY) const {
  // Portrait logical (W=panelH, H=panelW) → panel, 90° clockwise.
  phyX = y;
  phyY = static_cast<int>(panelHeight) - 1 - x;
}

void Gfx::clear(const bool black) { panel.clearScreen(black ? 0x00 : 0xFF); }

void Gfx::drawPixel(const int x, const int y, const bool black) {
  if (!fb) {
    return;
  }
  int phyX = 0;
  int phyY = 0;
  toPanel(x, y, phyX, phyY);
  if (phyX < 0 || phyX >= panelWidth || phyY < 0 || phyY >= panelHeight) {
    return;
  }
  const uint32_t byteIndex = static_cast<uint32_t>(phyY) * panelWidthBytes + static_cast<uint32_t>(phyX / 8);
  const uint8_t bit = static_cast<uint8_t>(7 - (phyX % 8));
  if (black) {
    fb[byteIndex] &= static_cast<uint8_t>(~(1u << bit));
  } else {
    fb[byteIndex] |= static_cast<uint8_t>(1u << bit);
  }
}

void Gfx::fillRect(int x, int y, int w, int h, const bool black) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > logicalWidth) {
    w = logicalWidth - x;
  }
  if (y + h > logicalHeight) {
    h = logicalHeight - y;
  }
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      drawPixel(xx, yy, black);
    }
  }
}

void Gfx::drawText(const int fontId, const int x, const int y, const char* text, const bool black,
                   const EpdFontFamily::Style style) {
  if (!text || *text == '\0') {
    return;
  }
  const EpdFontFamily* family = font(fontId);
  if (!family) {
    LOG_ERR("GFX", "Font %d missing", fontId);
    return;
  }
  const int yPos = y + family->getData(style)->ascender;
  int cursorX = x;
  int32_t prevAdvanceFP = 0;
  uint32_t prevCp = 0;
  const char* cursor = text;
  uint32_t cp = 0;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor)))) {
    if (utf8IsCombiningMark(cp)) {
      continue;
    }
    cp = family->applyLigatures(cp, cursor, style);
    if (prevCp != 0) {
      cursorX += fp4::toPixel(prevAdvanceFP + family->getKerning(prevCp, cp, style));
    }
    const EpdGlyph* glyph = family->getGlyph(cp, style);
    prevAdvanceFP = glyph ? glyph->advanceX : 0;
    if (glyph && glyph->width > 0 && glyph->height > 0) {
      const uint8_t* bitmap = &family->getData(style)->bitmap[glyph->dataOffset];
      const int gx0 = cursorX + glyph->left;
      const int gy0 = yPos - glyph->top;
      int pixelPosition = 0;
      for (int gy = 0; gy < glyph->height; ++gy) {
        for (int gx = 0; gx < glyph->width; ++gx, ++pixelPosition) {
          const uint8_t byte = bitmap[pixelPosition >> 3];
          const uint8_t bit = static_cast<uint8_t>(7 - (pixelPosition & 7));
          if ((byte >> bit) & 1) {
            drawPixel(gx0 + gx, gy0 + gy, black);
          }
        }
      }
    }
    prevCp = cp;
  }
}

void Gfx::drawCenteredText(const int fontId, const int y, const char* text, const bool black,
                           const EpdFontFamily::Style style) {
  const int w = textWidth(fontId, text, style);
  drawText(fontId, (logicalWidth - w) / 2, y, text, black, style);
}

int Gfx::textWidth(const int fontId, const char* text, const EpdFontFamily::Style style) const {
  const EpdFontFamily* family = font(fontId);
  if (!family || !text) {
    return 0;
  }
  int w = 0;
  int h = 0;
  family->getTextDimensions(text, &w, &h, style);
  return w;
}

int Gfx::lineHeight(const int fontId) const {
  const EpdFontFamily* family = font(fontId);
  if (!family) {
    return 16;
  }
  return family->getData()->advanceY;
}

void Gfx::present(const HalDisplay::RefreshMode mode) { panel.displayBuffer(mode); }
