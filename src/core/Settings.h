#pragma once

#include <cstdint>

struct Settings {
  static constexpr uint32_t MAGIC = 0x48585443;  // "HXTC"
  static constexpr uint16_t VERSION = 2;
  static constexpr const char* kDir = "/.lazahata";
  static constexpr const char* kPath = "/.lazahata/settings.bin";

  uint32_t magic = MAGIC;
  uint16_t version = VERSION;
  uint8_t sleepTimeoutMinutes = 5;
  uint8_t refreshEveryNPages = 5;
  uint8_t nightMode = 0;
  uint8_t reserved = 0;
  char lastBookPath[200]{};

  void load();
  void save() const;
  unsigned long sleepTimeoutMs() const;
};

extern Settings settings;
