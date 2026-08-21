#pragma once

#include <string>
#include <vector>

#include "core/Activity.h"

class BrowserScreen final : public Activity {
  char path[256] = "/";
  std::vector<std::string> entries;
  int index = 0;
  int window = 0;

  void load();
  void activate();
  void goUp();

 public:
  BrowserScreen(Gfx& gfx, MappedInput& input, const char* initialPath = "/");
  void onEnter() override;
  void loop() override;
  void render() override;
};
