#pragma once

#include <EpdFontFamily.h>
#include <HalDisplay.h>

// Portrait drawing surface. The X3 panel is 792×528 landscape; logical size is 528×792.
class Gfx {
 public:
  explicit Gfx(HalDisplay& display);

  void begin();

  int width() const { return logicalWidth; }
  int height() const { return logicalHeight; }
  uint16_t stride() const { return panelWidthBytes; }

  void insertFont(int id, const EpdFontFamily* family);
  const EpdFontFamily* font(int id) const;

  void clear(bool black = false);
  void drawPixel(int x, int y, bool black);
  void fillRect(int x, int y, int w, int h, bool black);
  void drawText(int fontId, int x, int y, const char* text, bool black = true,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR);
  void drawCenteredText(int fontId, int y, const char* text, bool black = true,
                        EpdFontFamily::Style style = EpdFontFamily::REGULAR);
  int textWidth(int fontId, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  int lineHeight(int fontId) const;

  void present(HalDisplay::RefreshMode mode = HalDisplay::FAST_REFRESH);

  uint8_t* frameBuffer() const { return fb; }

  // 2-bit grayscale helpers used by XtchBook::drawPage. Framebuffer is scratch between passes.
  void displayGrayscaleBase(HalDisplay::RefreshMode fallback = HalDisplay::HALF_REFRESH);
  void preconditionGrayscale();
  void copyGrayscaleLsbBuffers();
  void copyGrayscaleMsbBuffers();
  void displayGrayBuffer();
  void cleanupGrayscaleBuffers();
  bool combinesGrayscaleBase() const;

 private:
  static constexpr int kMaxFonts = 4;

  HalDisplay& panel;
  uint8_t* fb = nullptr;
  uint16_t panelWidth = 0;
  uint16_t panelHeight = 0;
  uint16_t panelWidthBytes = 0;
  int logicalWidth = 0;
  int logicalHeight = 0;

  struct FontSlot {
    int id = 0;
    const EpdFontFamily* family = nullptr;
  };
  FontSlot fonts[kMaxFonts]{};

  void toPanel(int x, int y, int& phyX, int& phyY) const;
};
