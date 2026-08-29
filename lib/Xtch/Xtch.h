#pragma once

#include <HalStorage.h>

#include <vector>

#include "XtchTypes.h"

class Gfx;

// Streaming XTCH reader. The source file stays open while the book is open so
// page turns do not re-walk FAT; close() releases it.
class XtchBook {
 public:
  XtchBook() = default;
  ~XtchBook();

  xtch::Error open(const char* path);
  void close();
  bool isOpen() const { return opened; }

  uint16_t pageCount() const { return header.pageCount; }
  uint16_t pageWidth() const { return defaultWidth; }
  uint16_t pageHeight() const { return defaultHeight; }
  const char* title() const { return bookTitle; }
  const char* author() const { return bookAuthor; }
  const char* path() const { return filepath; }
  xtch::Error lastError() const { return error; }

  bool pageInfo(uint32_t pageIndex, xtch::PageInfo& info);

  // Header flag only; getChapters() parses once and caches.
  bool hasChapters() const { return chaptersAvailable; }
  const std::vector<xtch::ChapterInfo>& getChapters();

  // Paints a 2-bit page and runs the display sequence; caller must not present() on success.
  // pagesUntilFullRefresh is decremented each call and reset to refreshFrequency at the periodic full refresh.
  bool drawPage(Gfx& gfx, uint32_t pageIndex, int& pagesUntilFullRefresh, int refreshFrequency);

 private:
  char filepath[256]{};
  char bookTitle[128]{};
  char bookAuthor[64]{};
  HalFile file;
  xtch::Header header{};
  uint16_t defaultWidth = 0;
  uint16_t defaultHeight = 0;
  bool opened = false;
  xtch::Error error = xtch::Error::Ok;
  std::vector<xtch::ChapterInfo> chapters;
  bool chaptersAvailable = false;
  bool chaptersLoaded = false;
  uint8_t* pageBuffer = nullptr;
  size_t pageBufferCapacity = 0;
  xtch::PageTableEntry* pageTable = nullptr;
  uint32_t* pageCluster = nullptr;
  uint16_t pageTableCount = 0;

  bool ensureOpen();
  void closeFile();
  xtch::Error readHeader();
  xtch::Error readMetadata();
  bool loadPageTable();
  bool readPageTableEntry(uint32_t pageIndex, xtch::PageInfo& info);
  void readChapters();
};
