#include "Xtch.h"

#include <Gfx.h>
#include <Logging.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

XtchBook::~XtchBook() { close(); }

void XtchBook::closeFile() {
  if (file.isOpen()) {
    file.close();
  }
}

void XtchBook::close() {
  closeFile();
  free(pageBuffer);
  pageBuffer = nullptr;
  pageBufferCapacity = 0;
  free(pageTable);
  pageTable = nullptr;
  free(pageCluster);
  pageCluster = nullptr;
  pageTableCount = 0;
  opened = false;
  defaultWidth = 0;
  defaultHeight = 0;
  bookTitle[0] = '\0';
  bookAuthor[0] = '\0';
  memset(&header, 0, sizeof(header));
  chapters.clear();
  chaptersAvailable = false;
  chaptersLoaded = false;
}

bool XtchBook::ensureOpen() {
  if (file.isOpen()) {
    return true;
  }
  return Storage.openFileForRead("XTCH", filepath, file);
}

xtch::Error XtchBook::open(const char* path) {
  close();
  if (!path || path[0] == '\0') {
    LOG_ERR("XTCH", "Empty path");
    error = xtch::Error::FileNotFound;
    return error;
  }
  snprintf(filepath, sizeof(filepath), "%s", path);

  if (!Storage.openFileForRead("XTCH", filepath, file)) {
    LOG_ERR("XTCH", "Not found %s", filepath);
    error = xtch::Error::FileNotFound;
    return error;
  }

  error = readHeader();
  if (error != xtch::Error::Ok) {
    closeFile();
    return error;
  }
  chaptersAvailable = (header.hasChapters == 1 && header.pageTableOffset >= sizeof(header));
  if (header.hasMetadata) {
    error = readMetadata();
    if (error != xtch::Error::Ok) {
      closeFile();
      return error;
    }
  }
  if (bookTitle[0] == '\0') {
    const char* slash = strrchr(filepath, '/');
    const char* name = slash ? slash + 1 : filepath;
    snprintf(bookTitle, sizeof(bookTitle), "%s", name);
  }

  if (!loadPageTable()) {
    closeFile();
    LOG_ERR("XTCH", "Page table unreadable");
    error = xtch::Error::CorruptedHeader;
    return error;
  }
  defaultWidth = pageTable[0].width;
  defaultHeight = pageTable[0].height;

  if (file.probeContiguous()) {
    LOG_DBG("XTCH", "File is contiguous");
  } else {
    LOG_DBG("XTCH", "File is not contiguous");
  }

  opened = true;
  LOG_INF("XTCH", "Opened %s (%u pages, %dx%d, title='%s')", filepath, header.pageCount, defaultWidth, defaultHeight,
          bookTitle);
  return xtch::Error::Ok;
}

xtch::Error XtchBook::readHeader() {
  uint8_t raw[sizeof(xtch::Header)];
  const size_t n = static_cast<size_t>(file.read(raw, sizeof(raw)));
  if (n != sizeof(raw)) {
    LOG_ERR("XTCH", "Short header read (%u of %u)", static_cast<unsigned>(n), static_cast<unsigned>(sizeof(raw)));
    return xtch::Error::ReadError;
  }
  memcpy(&header, raw, sizeof(header));

  if (header.magic != xtch::XTCH_MAGIC) {
    LOG_ERR("XTCH", "Bad magic 0x%08lX (need XTCH)", static_cast<unsigned long>(header.magic));
    return xtch::Error::InvalidMagic;
  }

  const bool validVersion =
      (header.versionMajor == 1 && header.versionMinor == 0) || (header.versionMajor == 0 && header.versionMinor == 1);
  if (!validVersion) {
    LOG_ERR("XTCH", "Unsupported version %u.%u", header.versionMajor, header.versionMinor);
    return xtch::Error::InvalidVersion;
  }
  if (header.pageCount == 0) {
    LOG_ERR("XTCH", "Header pageCount is 0");
    return xtch::Error::CorruptedHeader;
  }
  return xtch::Error::Ok;
}

xtch::Error XtchBook::readMetadata() {
  char titleBuf[sizeof(bookTitle)] = {};
  if (!file.seek(0x38)) {
    LOG_ERR("XTCH", "Seek title failed");
    return xtch::Error::ReadError;
  }
  file.read(titleBuf, sizeof(titleBuf) - 1);
  snprintf(bookTitle, sizeof(bookTitle), "%s", titleBuf);

  char authorBuf[sizeof(bookAuthor)] = {};
  if (!file.seek(0xB8)) {
    LOG_ERR("XTCH", "Seek author failed");
    return xtch::Error::ReadError;
  }
  file.read(authorBuf, sizeof(authorBuf) - 1);
  snprintf(bookAuthor, sizeof(bookAuthor), "%s", authorBuf);
  return xtch::Error::Ok;
}

bool XtchBook::loadPageTable() {
  free(pageTable);
  pageTable = nullptr;
  free(pageCluster);
  pageCluster = nullptr;
  pageTableCount = 0;
  if (header.pageCount == 0) {
    return false;
  }
  const size_t bytes = static_cast<size_t>(header.pageCount) * sizeof(xtch::PageTableEntry);
  pageTable = static_cast<xtch::PageTableEntry*>(malloc(bytes));
  if (!pageTable) {
    LOG_ERR("XTCH", "Failed to cache page table (%u bytes)", static_cast<unsigned>(bytes));
    return false;
  }
  if (!file.seek64(header.pageTableOffset) ||
      static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(pageTable), bytes)) != bytes) {
    LOG_ERR("XTCH", "Page table read failed");
    free(pageTable);
    pageTable = nullptr;
    return false;
  }
  pageCluster = static_cast<uint32_t*>(calloc(header.pageCount, sizeof(uint32_t)));
  if (!pageCluster) {
    LOG_ERR("XTCH", "Failed to cache page clusters");
    free(pageTable);
    pageTable = nullptr;
    return false;
  }
  pageTableCount = header.pageCount;
  LOG_DBG("XTCH", "Page table cached %u entries (%u bytes)", pageTableCount, static_cast<unsigned>(bytes));
  return true;
}

bool XtchBook::readPageTableEntry(const uint32_t pageIndex, xtch::PageInfo& info) {
  if (pageIndex >= header.pageCount) {
    return false;
  }
  if (pageTable && pageIndex < pageTableCount) {
    const xtch::PageTableEntry& entry = pageTable[pageIndex];
    info.offset = entry.dataOffset;
    info.size = entry.dataSize;
    info.width = entry.width;
    info.height = entry.height;
    return true;
  }
  if (!ensureOpen()) {
    return false;
  }
  const uint64_t entryOffset =
      header.pageTableOffset + static_cast<uint64_t>(pageIndex) * sizeof(xtch::PageTableEntry);
  if (!file.seek64(entryOffset)) {
    return false;
  }
  xtch::PageTableEntry entry{};
  if (static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(entry))) != sizeof(entry)) {
    return false;
  }
  info.offset = entry.dataOffset;
  info.size = entry.dataSize;
  info.width = entry.width;
  info.height = entry.height;
  return true;
}

bool XtchBook::pageInfo(const uint32_t pageIndex, xtch::PageInfo& info) { return readPageTableEntry(pageIndex, info); }

// 96-byte entries at chapterOffset|padding<<32: 80-byte name, startPage, endPage, 12 reserved.
void XtchBook::readChapters() {
  chapters.clear();
  chaptersLoaded = true;
  if (!chaptersAvailable) {
    return;
  }
  const uint64_t chapterOffset =
      static_cast<uint64_t>(header.chapterOffset) | (static_cast<uint64_t>(header.padding) << 32);
  if (chapterOffset == 0) {
    chaptersAvailable = false;
    return;
  }
  if (!ensureOpen()) {
    chaptersAvailable = false;
    return;
  }
  constexpr uint64_t kChapterEntrySize = 96;
  const uint64_t fileSize = file.fileSize64();
  if (chapterOffset < sizeof(header) || chapterOffset >= fileSize || chapterOffset + kChapterEntrySize > fileSize) {
    chaptersAvailable = false;
    closeFile();
    return;
  }

  // Clamp to whichever known table follows the chapter table so a bogus
  // header can't inflate the derived chapter count.
  uint64_t maxOffset = fileSize;
  if (header.pageTableOffset > chapterOffset && header.pageTableOffset <= fileSize) {
    maxOffset = header.pageTableOffset;
  } else if (header.dataOffset > chapterOffset && header.dataOffset <= fileSize) {
    maxOffset = header.dataOffset;
  }
  if (maxOffset <= chapterOffset || !file.seek64(chapterOffset)) {
    chaptersAvailable = false;
    closeFile();
    return;
  }

  const size_t chapterCount = static_cast<size_t>((maxOffset - chapterOffset) / kChapterEntrySize);
  if (chapterCount == 0 || chapterCount > header.pageCount) {
    if (chapterCount > header.pageCount) {
      LOG_ERR("XTCH", "Chapter count %u exceeds pageCount %u", static_cast<unsigned>(chapterCount), header.pageCount);
    }
    chaptersAvailable = false;
    closeFile();
    return;
  }

  chapters.reserve(chapterCount);
  uint8_t buf[kChapterEntrySize];
  for (size_t i = 0; i < chapterCount; ++i) {
    if (static_cast<size_t>(file.read(buf, sizeof(buf))) != sizeof(buf)) {
      break;
    }
    char nameBuf[81];
    memcpy(nameBuf, buf, 80);
    nameBuf[80] = '\0';
    std::string name(nameBuf, strnlen(nameBuf, 80));

    uint16_t startPage = 0;
    uint16_t endPage = 0;
    memcpy(&startPage, buf + 0x50, sizeof(startPage));
    memcpy(&endPage, buf + 0x52, sizeof(endPage));
    if (name.empty() && startPage == 0 && endPage == 0) {
      break;  // trailing unused entries
    }

    // On-disk pages are 1-based; PageInfo/drawPage use 0-based indices.
    if (startPage > 0) {
      --startPage;
    }
    if (endPage > 0) {
      --endPage;
    }
    if (startPage >= header.pageCount) {
      continue;
    }
    if (endPage >= header.pageCount) {
      endPage = header.pageCount - 1;
    }
    if (startPage > endPage) {
      continue;
    }
    chapters.push_back(xtch::ChapterInfo{std::move(name), startPage, endPage});
  }
  chaptersAvailable = !chapters.empty();
  closeFile();
  LOG_DBG("XTCH", "Chapters: %u", static_cast<unsigned>(chapters.size()));
}

const std::vector<xtch::ChapterInfo>& XtchBook::getChapters() {
  if (!chaptersLoaded) {
    readChapters();
  }
  return chapters;
}

namespace {
enum class PlaneOp : uint8_t { Ink, Lsb, Msb };

// Full-frame X3 pages are 528×792 logical = 792×528 panel. XTCH columns are
// stored in the same order as panel rows after the 90° map, so each source
// byte is already one framebuffer byte.
void blitFullFrame(uint8_t* fb, const uint8_t* plane1, const uint8_t* plane2, const size_t colBytes,
                   const uint16_t rows, const PlaneOp op) {
  for (uint16_t row = 0; row < rows; ++row) {
    const uint8_t* p1 = plane1 + static_cast<size_t>(row) * colBytes;
    const uint8_t* p2 = plane2 + static_cast<size_t>(row) * colBytes;
    uint8_t* dst = fb + static_cast<size_t>(row) * colBytes;
    switch (op) {
      case PlaneOp::Ink:
        for (size_t k = 0; k < colBytes; ++k) {
          dst[k] = static_cast<uint8_t>(~(p1[k] | p2[k]));
        }
        break;
      case PlaneOp::Lsb:
        for (size_t k = 0; k < colBytes; ++k) {
          dst[k] = static_cast<uint8_t>(static_cast<uint8_t>(~p1[k]) & p2[k]);
        }
        break;
      case PlaneOp::Msb:
        for (size_t k = 0; k < colBytes; ++k) {
          dst[k] = static_cast<uint8_t>(p1[k] ^ p2[k]);
        }
        break;
    }
  }
}
}  // namespace

bool XtchBook::drawPage(Gfx& gfx, const uint32_t pageIndex, int& pagesUntilFullRefresh, const int refreshFrequency) {
  const uint32_t tStart = millis();
  if (!opened) {
    LOG_ERR("XTCH", "drawPage but book is closed");
    error = xtch::Error::FileNotFound;
    return false;
  }

  auto fail = [this](const xtch::Error err) {
    closeFile();
    error = err;
    return false;
  };

  xtch::PageInfo page{};
  if (!readPageTableEntry(pageIndex, page)) {
    LOG_ERR("XTCH", "Page %lu out of range or unreadable", static_cast<unsigned long>(pageIndex));
    return fail(xtch::Error::PageOutOfRange);
  }
  if (static_cast<int>(page.width) > gfx.width() || static_cast<int>(page.height) > gfx.height()) {
    LOG_ERR("XTCH", "Page %lu is %ux%u, screen %dx%d", static_cast<unsigned long>(pageIndex), page.width, page.height,
            gfx.width(), gfx.height());
    return fail(xtch::Error::TooLarge);
  }
  if (!ensureOpen()) {
    LOG_ERR("XTCH", "Reopen failed for page %lu", static_cast<unsigned long>(pageIndex));
    return fail(xtch::Error::FileNotFound);
  }

  const uint16_t pageWidth = page.width;
  const uint16_t pageHeight = page.height;
  const size_t colBytes = (static_cast<size_t>(pageHeight) + 7) / 8;
  const size_t planeSize = colBytes * static_cast<size_t>(pageWidth);
  const size_t bitmapSize = planeSize * 2;
  const size_t totalRead = sizeof(xtch::PageHeader) + bitmapSize;

  if (pageBufferCapacity < totalRead) {
    free(pageBuffer);
    pageBuffer = static_cast<uint8_t*>(malloc(totalRead));
    pageBufferCapacity = pageBuffer ? totalRead : 0;
  }
  if (!pageBuffer) {
    LOG_ERR("XTCH", "Failed to allocate page buffer (%lu bytes)", static_cast<unsigned long>(totalRead));
    return fail(xtch::Error::OutOfMemory);
  }
  const uint32_t cachedCluster = (pageCluster && pageIndex < pageTableCount) ? pageCluster[pageIndex] : 0;
  if (cachedCluster != 0) {
    file.setPos(page.offset, cachedCluster);
  } else if (!file.seek64(page.offset)) {
    LOG_ERR("XTCH", "Seek page %lu offset %llu failed", static_cast<unsigned long>(pageIndex),
            static_cast<unsigned long long>(page.offset));
    return fail(xtch::Error::ReadError);
  } else if (pageCluster && pageIndex < pageTableCount) {
    uint64_t pos = 0;
    uint32_t cluster = 0;
    if (file.getPos(&pos, &cluster)) {
      pageCluster[pageIndex] = cluster;
    }
  }
  const size_t bytesRead = static_cast<size_t>(file.read(pageBuffer, totalRead));
  if (bytesRead != totalRead) {
    LOG_ERR("XTCH", "Short page read for page %lu: got %lu of %lu", static_cast<unsigned long>(pageIndex),
            static_cast<unsigned long>(bytesRead), static_cast<unsigned long>(totalRead));
    closeFile();
    error = xtch::Error::ReadError;
    return false;
  }

  xtch::PageHeader pageHeader{};
  memcpy(&pageHeader, pageBuffer, sizeof(pageHeader));
  if (pageHeader.magic != xtch::XTH_MAGIC) {
    LOG_ERR("XTCH", "Bad page magic 0x%08lX (need XTH)", static_cast<unsigned long>(pageHeader.magic));
    return fail(xtch::Error::InvalidMagic);
  }
  if (pageHeader.compression != 0 || pageHeader.dataSize != bitmapSize || pageHeader.width != pageWidth ||
      pageHeader.height != pageHeight) {
    LOG_ERR("XTCH", "Unsupported page %lu encoding: compression=%u dataSize=%lu expected=%lu",
            static_cast<unsigned long>(pageIndex), pageHeader.compression,
            static_cast<unsigned long>(pageHeader.dataSize), static_cast<unsigned long>(bitmapSize));
    return fail(xtch::Error::CorruptedHeader);
  }

  const uint32_t tLoaded = millis();
  LOG_DBG("XTCH", "Page %lu: SD load took %lums (%lu bytes, heap=%u)", static_cast<unsigned long>(pageIndex),
          static_cast<unsigned long>(tLoaded - tStart), static_cast<unsigned long>(bytesRead),
          static_cast<unsigned>(ESP.getFreeHeap()));

  const uint8_t* plane1 = pageBuffer + sizeof(xtch::PageHeader);
  const uint8_t* plane2 = plane1 + planeSize;
  const int ox = (gfx.width() - static_cast<int>(pageWidth)) / 2;
  const int oy = (gfx.height() - static_cast<int>(pageHeight)) / 2;
  uint8_t* fb = gfx.frameBuffer();
  const bool fullFrame = fb != nullptr && ox == 0 && oy == 0 && static_cast<int>(pageWidth) == gfx.width() &&
                         static_cast<int>(pageHeight) == gfx.height() && (pageHeight % 8) == 0 &&
                         gfx.stride() == static_cast<uint16_t>(colBytes);

  auto paint = [&](const PlaneOp op) {
    if (fullFrame) {
      blitFullFrame(fb, plane1, plane2, colBytes, pageWidth, op);
      return;
    }
    const bool ink = op == PlaneOp::Ink;
    gfx.clear(!ink);
    for (uint16_t y = 0; y < pageHeight; ++y) {
      const size_t byteInCol = y / 8;
      const uint8_t bitInByte = static_cast<uint8_t>(7 - (y % 8));
      size_t byteOffset = static_cast<size_t>(pageWidth - 1) * colBytes + byteInCol;
      for (uint16_t x = 0; x < pageWidth; ++x, byteOffset -= colBytes) {
        const uint8_t pv = static_cast<uint8_t>(((plane1[byteOffset] >> bitInByte) & 1) << 1 |
                                               ((plane2[byteOffset] >> bitInByte) & 1));
        const bool keep = op == PlaneOp::Ink ? pv >= 1 : op == PlaneOp::Lsb ? pv == 1 : (pv == 1 || pv == 2);
        if (keep) {
          gfx.drawPixel(ox + x, oy + y, ink);
        }
      }
    }
  };

  paint(PlaneOp::Ink);

  const uint32_t tPass1Decoded = millis();

  if (pagesUntilFullRefresh <= 1) {
    // Clean base before gray planes so ghosting doesn't accumulate.
    if (gfx.combinesGrayscaleBase()) {
      gfx.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
    } else {
      gfx.present(HalDisplay::HALF_REFRESH);
      gfx.preconditionGrayscale();
    }
    pagesUntilFullRefresh = refreshFrequency;
  } else {
    gfx.displayGrayscaleBase(HalDisplay::FAST_REFRESH);
    --pagesUntilFullRefresh;
  }

  const uint32_t tBwDisplayed = millis();

  paint(PlaneOp::Lsb);
  gfx.copyGrayscaleLsbBuffers();

  const uint32_t tLsbCopied = millis();

  paint(PlaneOp::Msb);
  gfx.copyGrayscaleMsbBuffers();

  const uint32_t tMsbCopied = millis();

  gfx.displayGrayBuffer();

  const uint32_t tGrayDisplayed = millis();

  // Rebuild BW baseline so the next differential update matches the panel.
  paint(PlaneOp::Ink);
  gfx.cleanupGrayscaleBuffers();

  error = xtch::Error::Ok;

  const uint32_t tRendered = millis();
  LOG_DBG("XTCH",
          "Rendered page %lu/%u: load=%lums pass1Decode=%lums bwDisplay=%lums lsbDecode+copy=%lums "
          "msbDecode+copy=%lums grayDisplay=%lums pass4+cleanup=%lums total=%lums",
          static_cast<unsigned long>(pageIndex + 1), header.pageCount,
          static_cast<unsigned long>(tLoaded - tStart), static_cast<unsigned long>(tPass1Decoded - tLoaded),
          static_cast<unsigned long>(tBwDisplayed - tPass1Decoded), static_cast<unsigned long>(tLsbCopied - tBwDisplayed),
          static_cast<unsigned long>(tMsbCopied - tLsbCopied), static_cast<unsigned long>(tGrayDisplayed - tMsbCopied),
          static_cast<unsigned long>(tRendered - tGrayDisplayed), static_cast<unsigned long>(tRendered - tStart));
  return true;
}
