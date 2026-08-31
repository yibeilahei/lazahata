#pragma once

#include <cstdint>
#include <string>

// XTCH on-disk layout (from CrossPoint lib/Xtc, MIT).
// Pages are pre-rendered 2-bit bitmaps. Width and height come from the page table.

namespace xtch {

constexpr uint32_t XTCH_MAGIC = 0x48435458;  // "XTCH"
constexpr uint32_t XTH_MAGIC = 0x00485458;   // "XTH\0"

#pragma pack(push, 1)
struct Header {
  uint32_t magic;
  uint8_t versionMajor;
  uint8_t versionMinor;
  uint16_t pageCount;
  uint8_t readDirection;
  uint8_t hasMetadata;
  uint8_t hasThumbnails;
  uint8_t hasChapters;
  uint32_t currentPage;
  uint64_t metadataOffset;
  uint64_t pageTableOffset;
  uint64_t dataOffset;
  uint64_t thumbOffset;
  uint32_t chapterOffset;
  uint32_t padding;
};

struct PageTableEntry {
  uint64_t dataOffset;
  uint32_t dataSize;
  uint16_t width;
  uint16_t height;
};

struct PageHeader {
  uint32_t magic;
  uint16_t width;
  uint16_t height;
  uint8_t colorMode;
  uint8_t compression;
  uint32_t dataSize;
  uint64_t md5;
};
#pragma pack(pop)

struct PageInfo {
  uint64_t offset;
  uint32_t size;
  uint16_t width;
  uint16_t height;
};

// One entry of the optional chapter table (96 bytes on disk: 80-byte name +
// 2-byte startPage + 2-byte endPage + 12 bytes reserved), pointed to by
// Header::chapterOffset/padding (combined as a uint64_t) when hasChapters==1.
struct ChapterInfo {
  std::string name;
  uint16_t startPage;  // 0-based, inclusive
  uint16_t endPage;    // 0-based, inclusive
};

enum class Error : uint8_t {
  Ok = 0,
  FileNotFound,
  InvalidMagic,
  InvalidVersion,
  CorruptedHeader,
  PageOutOfRange,
  ReadError,
  TooLarge,
  OutOfMemory,
  DecodeFailed,
};

inline const char* errorName(Error err) {
  switch (err) {
    case Error::Ok:
      return "OK";
    case Error::FileNotFound:
      return "file not found";
    case Error::InvalidMagic:
      return "invalid magic";
    case Error::InvalidVersion:
      return "unsupported version";
    case Error::CorruptedHeader:
      return "corrupted header";
    case Error::PageOutOfRange:
      return "page out of range";
    case Error::ReadError:
      return "read error";
    case Error::TooLarge:
      return "page larger than screen";
    case Error::OutOfMemory:
      return "out of memory";
    case Error::DecodeFailed:
      return "page decompression failed";
    default:
      return "unknown";
  }
}

}  // namespace xtch
