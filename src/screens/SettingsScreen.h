#pragma once

#include "Activity.h"

class SettingsScreen final : public Activity {
  int index = 0;

 public:
  SettingsScreen(Gfx& gfx, MappedInput& input) : Activity("Settings", gfx, input) {}
  void loop() override;
  void render() override;
};
