#pragma once

#include "core/Activity.h"
#include "core/SdUpdate.h"

class UpdateScreen final : public Activity {
  char firmwarePath[256]{};
  sdUpdate::Check check{};
  enum class State { Confirm, Updating, Failed } state = State::Confirm;
  bool started = false;
  int lastPercent = -1;
  const char* error = nullptr;

 public:
  void drawProgress(size_t written, size_t total);
  UpdateScreen(Gfx& gfx, MappedInput& input, const char* path);
  void onEnter() override;
  void loop() override;
  void render() override;
  bool blocksSleep() const override { return true; }
};
