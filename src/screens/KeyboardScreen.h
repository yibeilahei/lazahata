#pragma once

#include <string>

#include "core/Screen.h"

class WifiListScreen;

// Button-navigated on-screen keyboard (no touch support on this hardware).
// Arrow keys move a cursor over a character grid; Confirm activates the
// selected key; Back cancels. Reports its result directly to the
// WifiListScreen that pushed it, mirroring ChapterSelectionScreen/ReaderScreen.
class KeyboardScreen final : public Screen {
  WifiListScreen& parent;
  std::string title;
  std::string text;
  size_t maxLength;
  bool passwordMode;

  bool shifted = false;
  bool symbolsMode = false;
  int selRow = 1;
  int selCol = 0;

  int rowLength(int row) const;
  char charAt(int row, int col) const;
  void activateKey();

 public:
  KeyboardScreen(Gfx& gfx, MappedInput& input, WifiListScreen& parent, const char* title, size_t maxLength,
                bool passwordMode);
  void loop() override;
  void render() override;
};
