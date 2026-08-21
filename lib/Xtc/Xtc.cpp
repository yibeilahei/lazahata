#include "Xtc.h"

#include <Gfx.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr size_t kMaxColBytes = 128;
}  // namespace

XtcBook::~XtcBook() { close(); }

void XtcBook::closeFile() {
  if (file.isOpen()) {
    file.close();
  }
}

void XtcBook::close() {
  closeFile();
  opened = false;
  defaultWidth = 0;
  defaultHeight = 0;
  bookTitle[0] = '\0';
  bookAuthor[0] = '\0';
  memset(&header, 0, sizeof(header));
}

bool XtcBook::ensureOpen() {
  if (file.isOpen()) {
    return true;
  }
  return Storage.openFileForRead("XTCH", filepath, file);
}

xtc::Error XtcBook::open(const char* path) {
  close();
  if (!path || path[0] == '\0') {
    error = xtc::Error::FileNotFound;
    return error;
  }
  snprintf(filepath, sizeof(filepath), "%s", path);

  if (!Storage.openFileForRead("XTCH", filepath, file)) {
    error = xtc::Error::FileNotFound;
    return error;
  }

  error = readHeader();
  if (error != xtc::Error::Ok) {
    closeFile();
    return error;
  }
  if (header.hasMetadata) {
    error = readMetadata();
    if (error != xtc::Error::Ok) {
      closeFile();
      return error;
    }
  }
  if (bookTitle[0] == '\0') {
    const char* slash = strrchr(filepath, '/');
    const char* name = slash ? slash + 1 : filepath;
    snprintf(bookTitle, sizeof(bookTitle), "%s", name);
  }

  xtc::PageInfo first{};
  if (!readPageTableEntry(0, first)) {
    closeFile();
    error = xtc::Error::CorruptedHeader;
    return error;
  }
  defaultWidth = first.width;
  defaultHeight = first.height;

  closeFile();
  opened = true;
  LOG_INF("XTCH", "Opened %s (%u pages, %dx%d, 2-bit)", filepath, header.pageCount, defaultWidth, defaultHeight);
  return xtc::Error::Ok;
}

xtc::Error XtcBook::readHeader() {
  uint8_t raw[sizeof(xtc::XtcHeader)];
  const size_t n = static_cast<size_t>(file.read(raw, sizeof(raw)));
  if (n != sizeof(raw)) {
    return xtc::Error::ReadError;
  }
  memcpy(&header, raw, sizeof(header));

  if (header.magic != xtc::XTCH_MAGIC) {
    LOG_ERR("XTCH", "Bad magic 0x%08lX (need XTCH)", static_cast<unsigned long>(header.magic));
    return xtc::Error::InvalidMagic;
  }

  const bool validVersion =
      (header.versionMajor == 1 && header.versionMinor == 0) || (header.versionMajor == 0 && header.versionMinor == 1);
  if (!validVersion) {
    LOG_ERR("XTCH", "Unsupported version %u.%u", header.versionMajor, header.versionMinor);
    return xtc::Error::InvalidVersion;
  }
  if (header.pageCount == 0) {
    return xtc::Error::CorruptedHeader;
  }
  return xtc::Error::Ok;
}

xtc::Error XtcBook::readMetadata() {
  char titleBuf[sizeof(bookTitle)] = {};
  if (!file.seek(0x38)) {
    return xtc::Error::ReadError;
  }
  file.read(titleBuf, sizeof(titleBuf) - 1);
  snprintf(bookTitle, sizeof(bookTitle), "%s", titleBuf);

  char authorBuf[sizeof(bookAuthor)] = {};
  if (!file.seek(0xB8)) {
    return xtc::Error::ReadError;
  }
  file.read(authorBuf, sizeof(authorBuf) - 1);
  snprintf(bookAuthor, sizeof(bookAuthor), "%s", authorBuf);
  return xtc::Error::Ok;
}

bool XtcBook::readPageTableEntry(const uint32_t pageIndex, xtc::PageInfo& info) {
  if (pageIndex >= header.pageCount) {
    return false;
  }
  if (!ensureOpen()) {
    return false;
  }
  const uint64_t entryOffset =
      header.pageTableOffset + static_cast<uint64_t>(pageIndex) * sizeof(xtc::PageTableEntry);
  if (!file.seek64(entryOffset)) {
    return false;
  }
  xtc::PageTableEntry entry{};
  if (static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(entry))) != sizeof(entry)) {
    return false;
  }
  info.offset = entry.dataOffset;
  info.size = entry.dataSize;
  info.width = entry.width;
  info.height = entry.height;
  return true;
}

bool XtcBook::pageInfo(const uint32_t pageIndex, xtc::PageInfo& info) { return readPageTableEntry(pageIndex, info); }

bool XtcBook::drawPage(Gfx& gfx, const uint32_t pageIndex) {
  if (!opened) {
    error = xtc::Error::FileNotFound;
    return false;
  }
  xtc::PageInfo page{};
  if (!readPageTableEntry(pageIndex, page)) {
    error = xtc::Error::PageOutOfRange;
    return false;
  }
  if (static_cast<int>(page.width) > gfx.width() || static_cast<int>(page.height) > gfx.height()) {
    error = xtc::Error::TooLarge;
    LOG_ERR("XTCH", "Page %lu is %ux%u, screen %dx%d", static_cast<unsigned long>(pageIndex), page.width, page.height,
            gfx.width(), gfx.height());
    return false;
  }
  if (!ensureOpen()) {
    error = xtc::Error::FileNotFound;
    return false;
  }
  if (!file.seek64(page.offset)) {
    error = xtc::Error::ReadError;
    return false;
  }

  xtc::XthPageHeader pageHeader{};
  if (static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(&pageHeader), sizeof(pageHeader))) !=
      sizeof(pageHeader)) {
    error = xtc::Error::ReadError;
    return false;
  }
  if (pageHeader.magic != xtc::XTH_MAGIC) {
    LOG_ERR("XTCH", "Bad page magic 0x%08lX (need XTH)", static_cast<unsigned long>(pageHeader.magic));
    error = xtc::Error::InvalidMagic;
    return false;
  }

  const int ox = (gfx.width() - static_cast<int>(pageHeader.width)) / 2;
  const int oy = (gfx.height() - static_cast<int>(pageHeader.height)) / 2;

  gfx.clear(false);

  const size_t colBytes = (static_cast<size_t>(pageHeader.height) + 7) / 8;
  if (colBytes > kMaxColBytes) {
    error = xtc::Error::TooLarge;
    return false;
  }
  const uint64_t dataStart = page.offset + sizeof(xtc::XthPageHeader);
  const size_t planeSize = colBytes * static_cast<size_t>(pageHeader.width);
  uint8_t plane1[kMaxColBytes];
  uint8_t plane2[kMaxColBytes];
  for (uint16_t x = 0; x < pageHeader.width; ++x) {
    const size_t colIndex = static_cast<size_t>(pageHeader.width) - 1 - x;
    if (!file.seek64(dataStart + colIndex * colBytes)) {
      error = xtc::Error::ReadError;
      return false;
    }
    if (static_cast<size_t>(file.read(plane1, colBytes)) != colBytes) {
      error = xtc::Error::ReadError;
      return false;
    }
    if (!file.seek64(dataStart + planeSize + colIndex * colBytes)) {
      error = xtc::Error::ReadError;
      return false;
    }
    if (static_cast<size_t>(file.read(plane2, colBytes)) != colBytes) {
      error = xtc::Error::ReadError;
      return false;
    }
    for (uint16_t y = 0; y < pageHeader.height; ++y) {
      const size_t byteInCol = y / 8;
      const uint8_t bit = static_cast<uint8_t>(7 - (y % 8));
      const uint8_t pv = static_cast<uint8_t>(((plane1[byteInCol] >> bit) & 1) << 1 | ((plane2[byteInCol] >> bit) & 1));
      if (pv >= 1) {
        gfx.drawPixel(ox + x, oy + y, true);
      }
    }
  }

  closeFile();
  error = xtc::Error::Ok;
  return true;
}
