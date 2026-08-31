#include "core/SdUpdate.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <mbedtls/sha256.h>
#include <spi_flash_mmap.h>

#include <cstring>

namespace {
constexpr size_t kChunk = 4096;
constexpr size_t kMinBytes = 64 * 1024;
constexpr size_t kHeader = 24;
constexpr size_t kSegHeader = 8;
constexpr size_t kShaTrailer = 32;
constexpr uint8_t kMagic = 0xE9;
constexpr uint8_t kChecksumSeed = 0xEF;
constexpr uint8_t kMaxSegments = 16;
constexpr size_t kBlk = 64 * 1024;
constexpr uint32_t kOtaImgNew = 0;
constexpr uint32_t kOtaImgInvalid = 3;
constexpr uint32_t kOtaImgAborted = 4;

uint8_t gBuf[kChunk];
const char* gError = "invalid firmware";

void setError(const char* e) { gError = e ? e : "invalid firmware"; }

const esp_partition_t* nextSlot() { return esp_ota_get_next_update_partition(nullptr); }

size_t partitionLimit() {
  const esp_partition_t* dest = nextSlot();
  if (dest && dest->size > 0) {
    return dest->size;
  }
  return 0x640000;
}

uint16_t runningChipId() {
  static uint16_t cached = 0xFFFF;
  static bool ready = false;
  if (ready) {
    return cached;
  }
  ready = true;
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (!run) {
    return cached;
  }
  uint16_t id = 0xFFFF;
  if (esp_partition_read(run, 12, &id, sizeof(id)) == ESP_OK) {
    cached = id;
  }
  return cached;
}

bool readExact(HalFile& file, void* dst, size_t n) {
  return file.read(dst, n) == static_cast<int>(n);
}

bool feed(HalFile& file, size_t length, uint8_t* xorAccum, mbedtls_sha256_context* sha) {
  size_t remaining = length;
  while (remaining > 0) {
    const size_t want = remaining > kChunk ? kChunk : remaining;
    if (!readExact(file, gBuf, want)) {
      return false;
    }
    if (sha) {
      mbedtls_sha256_update(sha, gBuf, want);
    }
    if (xorAccum) {
      uint8_t acc = *xorAccum;
      for (size_t i = 0; i < want; ++i) {
        acc ^= gBuf[i];
      }
      *xorAccum = acc;
    }
    remaining -= want;
    feedLoopWDT();
  }
  return true;
}

struct __attribute__((packed)) OtaSelect {
  uint32_t otaSeq;
  uint8_t seqLabel[20];
  uint32_t otaState;
  uint32_t crc;
};
static_assert(sizeof(OtaSelect) == 32, "OtaSelect must be 32 bytes");

uint32_t seqCrc(uint32_t seq) {
  return esp_rom_crc32_le(UINT32_MAX, reinterpret_cast<const uint8_t*>(&seq), sizeof(seq));
}

bool switchBoot(const esp_partition_t* dest) {
  if (!dest) {
    return false;
  }
  const esp_partition_t* otadata =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!otadata || otadata->size < 2 * SPI_FLASH_SEC_SIZE) {
    LOG_ERR("FW", "otadata missing");
    return false;
  }

  OtaSelect slots[2] = {};
  if (esp_partition_read(otadata, 0, &slots[0], sizeof(OtaSelect)) != ESP_OK ||
      esp_partition_read(otadata, SPI_FLASH_SEC_SIZE, &slots[1], sizeof(OtaSelect)) != ESP_OK) {
    LOG_ERR("FW", "otadata read failed");
    return false;
  }

  int activeIdx = -1;
  uint32_t activeSeq = 0;
  for (int i = 0; i < 2; ++i) {
    if (slots[i].otaSeq == 0xFFFFFFFFu) {
      continue;
    }
    if (slots[i].crc != seqCrc(slots[i].otaSeq)) {
      continue;
    }
    if (slots[i].otaState == kOtaImgInvalid || slots[i].otaState == kOtaImgAborted) {
      continue;
    }
    if (activeIdx < 0 || slots[i].otaSeq > activeSeq) {
      activeIdx = i;
      activeSeq = slots[i].otaSeq;
    }
  }

  const uint32_t destIdx =
      static_cast<uint32_t>(dest->subtype) - static_cast<uint32_t>(ESP_PARTITION_SUBTYPE_APP_OTA_0);
  if (destIdx > 15) {
    LOG_ERR("FW", "dest is not an OTA app");
    return false;
  }

  uint32_t newSeq = activeSeq + 1;
  while (((newSeq - 1u) % 2u) != (destIdx % 2u)) {
    ++newSeq;
  }

  OtaSelect next = {};
  next.otaSeq = newSeq;
  memset(next.seqLabel, 0xFF, sizeof(next.seqLabel));
  next.otaState = kOtaImgNew;
  next.crc = seqCrc(next.otaSeq);

  const int targetSlot = (activeIdx == 0) ? 1 : 0;
  const size_t targetOff = static_cast<size_t>(targetSlot) * SPI_FLASH_SEC_SIZE;
  if (esp_partition_erase_range(otadata, targetOff, SPI_FLASH_SEC_SIZE) != ESP_OK ||
      esp_partition_write(otadata, targetOff, &next, sizeof(next)) != ESP_OK) {
    LOG_ERR("FW", "otadata write failed");
    return false;
  }
  LOG_INF("FW", "otadata seq=%u -> %s", static_cast<unsigned>(newSeq), dest->label);
  return true;
}

sdUpdate::Check fail(sdUpdate::Check out, const char* error) {
  setError(error);
  out.ok = false;
  out.error = gError;
  return out;
}
}  // namespace

const char* sdUpdate::lastError() { return gError; }

sdUpdate::Check sdUpdate::inspect(const char* path) {
  Check out;
  if (!path || path[0] == '\0' || !Storage.exists(path)) {
    return fail(out, "file not found");
  }
  HalFile file;
  if (!Storage.openFileForRead("FW", path, file) || !file) {
    return fail(out, "could not open file");
  }
  out.size = file.fileSize();
  if (out.size < kMinBytes) {
    file.close();
    return fail(out, "file too small");
  }
  const size_t limit = partitionLimit();
  if (out.size > limit) {
    file.close();
    return fail(out, "file too large");
  }

  uint8_t header[kHeader];
  if (!readExact(file, header, kHeader)) {
    file.close();
    return fail(out, "could not read file");
  }
  if (header[0] != kMagic) {
    file.close();
    return fail(out, "not an ESP32 image");
  }
  uint16_t imageChip = 0;
  memcpy(&imageChip, header + 12, sizeof(imageChip));
  const uint16_t deviceChip = runningChipId();
  if (deviceChip != 0xFFFF && imageChip != deviceChip) {
    LOG_ERR("FW", "wrong chip image=0x%04X device=0x%04X", imageChip, deviceChip);
    file.close();
    return fail(out, "wrong device");
  }
  const uint8_t segCount = header[1];
  if (segCount == 0 || segCount > kMaxSegments) {
    file.close();
    return fail(out, "invalid firmware");
  }
  const bool hashAppended = header[23] != 0;

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, header, kHeader);

  uint8_t xorAccum = kChecksumSeed;
  size_t pos = kHeader;
  for (uint8_t i = 0; i < segCount; ++i) {
    if (pos + kSegHeader > out.size) {
      mbedtls_sha256_free(&sha);
      file.close();
      return fail(out, "invalid firmware");
    }
    uint8_t segHdr[kSegHeader];
    if (!readExact(file, segHdr, kSegHeader)) {
      mbedtls_sha256_free(&sha);
      file.close();
      return fail(out, "could not read file");
    }
    mbedtls_sha256_update(&sha, segHdr, kSegHeader);
    pos += kSegHeader;
    uint32_t dataLen = 0;
    memcpy(&dataLen, segHdr + 4, sizeof(dataLen));
    if (pos + dataLen > out.size) {
      mbedtls_sha256_free(&sha);
      file.close();
      return fail(out, "invalid firmware");
    }
    if (!feed(file, dataLen, &xorAccum, &sha)) {
      mbedtls_sha256_free(&sha);
      file.close();
      return fail(out, "could not read file");
    }
    pos += dataLen;
  }

  const size_t padEnd = (pos + 16) & ~static_cast<size_t>(15);
  const size_t expected = padEnd + (hashAppended ? kShaTrailer : 0);
  if (expected != out.size) {
    LOG_ERR("FW", "size mismatch expected=%u actual=%u", static_cast<unsigned>(expected),
            static_cast<unsigned>(out.size));
    mbedtls_sha256_free(&sha);
    file.close();
    return fail(out, "invalid firmware");
  }
  const size_t padLen = padEnd - pos;
  uint8_t padBuf[16];
  if (padLen == 0 || padLen > sizeof(padBuf) || !readExact(file, padBuf, padLen)) {
    mbedtls_sha256_free(&sha);
    file.close();
    return fail(out, "invalid firmware");
  }
  mbedtls_sha256_update(&sha, padBuf, padLen);
  if (xorAccum != padBuf[padLen - 1]) {
    LOG_ERR("FW", "checksum mismatch");
    mbedtls_sha256_free(&sha);
    file.close();
    return fail(out, "invalid firmware");
  }
  if (hashAppended) {
    uint8_t computed[kShaTrailer];
    uint8_t stored[kShaTrailer];
    mbedtls_sha256_finish(&sha, computed);
    if (!readExact(file, stored, kShaTrailer) || memcmp(computed, stored, kShaTrailer) != 0) {
      LOG_ERR("FW", "SHA256 mismatch");
      mbedtls_sha256_free(&sha);
      file.close();
      return fail(out, "invalid firmware");
    }
  }
  mbedtls_sha256_free(&sha);
  file.close();
  setError("invalid firmware");
  out.ok = true;
  out.error = nullptr;
  return out;
}

bool sdUpdate::flash(const char* path) {
  setError("write failed");
  const Check check = inspect(path);
  if (!check.ok) {
    LOG_ERR("FW", "Reject %s: %s", path ? path : "", check.error ? check.error : "");
    return false;
  }

  const esp_partition_t* dest = nextSlot();
  if (!dest || dest == esp_ota_get_running_partition()) {
    LOG_ERR("FW", "no next OTA slot");
    setError("write failed");
    return false;
  }

  HalFile file;
  if (!Storage.openFileForRead("FW", path, file) || !file) {
    LOG_ERR("FW", "Reopen failed %s", path);
    setError("could not open file");
    return false;
  }

  LOG_INF("FW", "Flashing %s (%u bytes) -> %s", path, static_cast<unsigned>(check.size), dest->label);
  size_t pos = 0;
  size_t erased = 0;
  while (pos < check.size) {
    if (pos >= erased) {
      size_t eraseLen = dest->size - pos;
      if (eraseLen > kBlk) {
        eraseLen = kBlk;
      }
      eraseLen = (eraseLen + SPI_FLASH_SEC_SIZE - 1) & ~(SPI_FLASH_SEC_SIZE - 1);
      if (eraseLen > dest->size - pos) {
        eraseLen = dest->size - pos;
      }
      if (esp_partition_erase_range(dest, pos, eraseLen) != ESP_OK) {
        LOG_ERR("FW", "erase @%u failed", static_cast<unsigned>(pos));
        file.close();
        setError("write failed");
        return false;
      }
      erased = pos + eraseLen;
    }
    const size_t want = (check.size - pos > kChunk) ? kChunk : (check.size - pos);
    if (!readExact(file, gBuf, want)) {
      LOG_ERR("FW", "Short read at %u", static_cast<unsigned>(pos));
      file.close();
      setError("could not read file");
      return false;
    }
    if (esp_partition_write(dest, pos, gBuf, want) != ESP_OK) {
      LOG_ERR("FW", "write @%u failed", static_cast<unsigned>(pos));
      file.close();
      setError("write failed");
      return false;
    }
    pos += want;
    feedLoopWDT();
    delay(1);
  }
  file.close();

  if (!switchBoot(dest)) {
    setError("write failed");
    return false;
  }
  LOG_INF("FW", "Flash ok");
  return true;
}
