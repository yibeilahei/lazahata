#pragma once

#include <vector>

#include <XtchTypes.h>

#include "core/Screen.h"

class ReaderScreen;

class ChapterSelectionScreen final : public Screen {
  ReaderScreen& reader;
  std::vector<xtch::ChapterInfo> chapters;
  int index = 0;
  int window = 0;

  void activate();

 public:
  ChapterSelectionScreen(Gfx& gfx, MappedInput& input, ReaderScreen& reader,
                         const std::vector<xtch::ChapterInfo>& chapterList, uint32_t currentPage);
  void loop() override;
  void render() override;
};
