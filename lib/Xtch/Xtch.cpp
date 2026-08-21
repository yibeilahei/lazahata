#include "Xtch.h"

#include <Gfx.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr size_t kMaxColBytes = 128;
}  // namespace

XtchBook::~XtchBook() { close(); }

void XtchBook::closeFile() {
  if (file.isOpen()) {
    file.close();
  }
}

void XtchBook::close() {
  closeFile();
  opened = false;
  defaultWidth = 0;
  defaultHeight = 0;
  bookTitle[0] = '\0';
  bookAuthor[0] = '\0';
  memset(&header, 0, sizeof(header));
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

  xtch::PageInfo first{};
  if (!readPageTableEntry(0, first)) {
    closeFile();
    LOG_ERR("XTCH", "Page table unreadable");
    error = xtch::Error::CorruptedHeader;
    return error;
  }
  defaultWidth = first.width;
  defaultHeight = first.height;

  closeFile();
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

bool XtchBook::readPageTableEntry(const uint32_t pageIndex, xtch::PageInfo& info) {
  if (pageIndex >= header.pageCount) {
    return false;
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

bool XtchBook::drawPage(Gfx& gfx, const uint32_t pageIndex) {
  if (!opened) {
    LOG_ERR("XTCH", "drawPage but book is closed");
    error = xtch::Error::FileNotFound;
    return false;
  }
  xtch::PageInfo page{};
  if (!readPageTableEntry(pageIndex, page)) {
    LOG_ERR("XTCH", "Page %lu out of range or unreadable", static_cast<unsigned long>(pageIndex));
    error = xtch::Error::PageOutOfRange;
    return false;
  }
  if (static_cast<int>(page.width) > gfx.width() || static_cast<int>(page.height) > gfx.height()) {
    error = xtch::Error::TooLarge;
    LOG_ERR("XTCH", "Page %lu is %ux%u, screen %dx%d", static_cast<unsigned long>(pageIndex), page.width, page.height,
            gfx.width(), gfx.height());
    return false;
  }
  if (!ensureOpen()) {
    LOG_ERR("XTCH", "Reopen failed for page %lu", static_cast<unsigned long>(pageIndex));
    error = xtch::Error::FileNotFound;
    return false;
  }
  if (!file.seek64(page.offset)) {
    LOG_ERR("XTCH", "Seek page %lu offset %llu failed", static_cast<unsigned long>(pageIndex),
            static_cast<unsigned long long>(page.offset));
    error = xtch::Error::ReadError;
    return false;
  }

  xtch::PageHeader pageHeader{};
  if (static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(&pageHeader), sizeof(pageHeader))) !=
      sizeof(pageHeader)) {
    LOG_ERR("XTCH", "Short page header at %lu", static_cast<unsigned long>(pageIndex));
    error = xtch::Error::ReadError;
    return false;
  }
  if (pageHeader.magic != xtch::XTH_MAGIC) {
    LOG_ERR("XTCH", "Bad page magic 0x%08lX (need XTH)", static_cast<unsigned long>(pageHeader.magic));
    error = xtch::Error::InvalidMagic;
    return false;
  }

  const int ox = (gfx.width() - static_cast<int>(pageHeader.width)) / 2;
  const int oy = (gfx.height() - static_cast<int>(pageHeader.height)) / 2;

  gfx.clear(false);

  const size_t colBytes = (static_cast<size_t>(pageHeader.height) + 7) / 8;
  if (colBytes > kMaxColBytes) {
    LOG_ERR("XTCH", "Page %lu colBytes %u too large", static_cast<unsigned long>(pageIndex),
            static_cast<unsigned>(colBytes));
    error = xtch::Error::TooLarge;
    return false;
  }
  const uint64_t dataStart = page.offset + sizeof(xtch::PageHeader);
  const size_t planeSize = colBytes * static_cast<size_t>(pageHeader.width);
  uint8_t plane1[kMaxColBytes];
  uint8_t plane2[kMaxColBytes];
  for (uint16_t x = 0; x < pageHeader.width; ++x) {
    const size_t colIndex = static_cast<size_t>(pageHeader.width) - 1 - x;
    if (!file.seek64(dataStart + colIndex * colBytes)) {
      LOG_ERR("XTCH", "Seek plane1 col %u page %lu failed", x, static_cast<unsigned long>(pageIndex));
      error = xtch::Error::ReadError;
      return false;
    }
    if (static_cast<size_t>(file.read(plane1, colBytes)) != colBytes) {
      LOG_ERR("XTCH", "Read plane1 col %u page %lu failed", x, static_cast<unsigned long>(pageIndex));
      error = xtch::Error::ReadError;
      return false;
    }
    if (!file.seek64(dataStart + planeSize + colIndex * colBytes)) {
      LOG_ERR("XTCH", "Seek plane2 col %u page %lu failed", x, static_cast<unsigned long>(pageIndex));
      error = xtch::Error::ReadError;
      return false;
    }
    if (static_cast<size_t>(file.read(plane2, colBytes)) != colBytes) {
      LOG_ERR("XTCH", "Read plane2 col %u page %lu failed", x, static_cast<unsigned long>(pageIndex));
      error = xtch::Error::ReadError;
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
  error = xtch::Error::Ok;
  return true;
}
