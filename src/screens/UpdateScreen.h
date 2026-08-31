#pragma once

#include "core/Screen.h"
#include "core/SdUpdate.h"

class UpdateScreen final : public Screen {
  char firmwarePath[256]{};
  sdUpdate::Check check{};
  enum class State { Confirm, Updating, Failed } state = State::Confirm;
  bool started = false;
  const char* error = nullptr;

 public:
  UpdateScreen(Gfx& gfx, MappedInput& input, const char* path);
  void onEnter() override;
  void loop() override;
  void render() override;
  bool blocksSleep() const override { return true; }
};
