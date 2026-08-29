#include "ChapterSelectionScreen.h"

#include <Gfx.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "core/UiList.h"
#include "core/fontIds.h"
#include "screens/ReaderScreen.h"

ChapterSelectionScreen::ChapterSelectionScreen(Gfx& gfx, MappedInput& input, ReaderScreen& reader,
                                               const std::vector<xtch::ChapterInfo>& chapterList,
                                               const uint32_t currentPage)
    : Screen("Chapters", gfx, input), reader(reader), chapters(chapterList) {
  for (size_t i = 0; i < chapters.size(); ++i) {
    if (currentPage >= chapters[i].startPage && currentPage <= chapters[i].endPage) {
      index = static_cast<int>(i);
      break;
    }
  }
}

void ChapterSelectionScreen::activate() {
  if (chapters.empty() || index < 0 || index >= static_cast<int>(chapters.size())) {
    return;
  }
  reader.jumpToPage(chapters[static_cast<size_t>(index)].startPage);
  finish();
}

void ChapterSelectionScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }
  const int count = static_cast<int>(chapters.size());
  if (ui::applyDelta(index, input.consumeNavigationDelta(), count)) {
    requestUpdate();
  } else if (count > 0 && input.wasReleased(MappedInput::Button::Confirm)) {
    activate();
  }
}

void ChapterSelectionScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 8, "Chapters");

  const int rowH = gfx.lineHeight(FONT_UI) + 8;
  const int top = 40;
  const int rows = (gfx.height() - top - 24) / rowH;
  ui::followWindow(window, index, rows);
  if (chapters.empty()) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2, "No chapters");
  } else {
    const int last = std::min(window + rows, static_cast<int>(chapters.size()));
    char label[112];
    for (int i = window; i < last; ++i) {
      const xtch::ChapterInfo& chapter = chapters[static_cast<size_t>(i)];
      if (chapter.name.empty()) {
        snprintf(label, sizeof(label), "Chapter %d (p%u-%u)", i + 1, chapter.startPage + 1, chapter.endPage + 1);
      } else {
        snprintf(label, sizeof(label), "%s (p%u-%u)", chapter.name.c_str(), chapter.startPage + 1,
                 chapter.endPage + 1);
      }
      ui::drawRow(gfx, top + (i - window) * rowH, rowH, label, i == index);
    }
  }
  presentUi();
}
