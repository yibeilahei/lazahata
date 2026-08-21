#pragma once

#include <Xtch.h>

#include "core/Activity.h"

class ReaderScreen final : public Activity {
  char bookPath[256]{};
  XtchBook book;
  uint32_t page = 0;
  int pagesUntilFull = 0;
  bool loaded = false;
  bool overlay = true;

  void loadProgress();
  void saveProgress() const;

 public:
  ReaderScreen(Gfx& gfx, MappedInput& input, const char* path);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render() override;
  bool isReader() const override { return true; }
};
