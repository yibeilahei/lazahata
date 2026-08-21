#pragma once

#include <string>
#include <vector>

#include "core/Activity.h"

class BrowserScreen final : public Activity {
 public:
  enum class Mode : uint8_t { Books, Firmware };

 private:
  char path[256] = "/";
  std::vector<std::string> entries;
  int index = 0;
  int window = 0;
  Mode mode = Mode::Books;
  bool lockRoot = false;

  void load();
  void activate();
  void goUp();

 public:
  BrowserScreen(Gfx& gfx, MappedInput& input, const char* initialPath = "/", Mode mode = Mode::Books,
                bool lockRoot = false);
  void onEnter() override;
  void loop() override;
  void render() override;
  bool blocksSleep() const override { return mode == Mode::Firmware; }
};
