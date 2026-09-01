#pragma once

#include <cstdint>
#include <string>

#include "core/Screen.h"

class ReaderScreen;

// Numeric keypad for typing a page number to jump to. Pushed from
// ChapterSelectionScreen's "Go to page" row. Confirm on the checkmark key
// commits (clamped to [1, pageCount]); Back cancels without changing anything.
class PageJumpScreen final : public Screen {
  ReaderScreen& reader;
  uint32_t currentPage;
  uint16_t pageCount;
  std::string digits;
  size_t maxDigits;

  int selRow = 0;
  int selCol = 0;

  int rowLength(int row) const;
  void activateKey();

 public:
  PageJumpScreen(Gfx& gfx, MappedInput& input, ReaderScreen& reader, uint32_t currentPage, uint16_t pageCount);
  void loop() override;
  void render() override;
};
