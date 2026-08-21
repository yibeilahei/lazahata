#pragma once

#include <HalStorage.h>

#include "XtchTypes.h"

class Gfx;

// Streaming XTCH reader. The source file is closed between reads so SdFat
// buffers are not held during a blit.
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
  bool drawPage(Gfx& gfx, uint32_t pageIndex);

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

  bool ensureOpen();
  void closeFile();
  xtch::Error readHeader();
  xtch::Error readMetadata();
  bool readPageTableEntry(uint32_t pageIndex, xtch::PageInfo& info);
};
