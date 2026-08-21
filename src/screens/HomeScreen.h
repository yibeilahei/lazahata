#pragma once

#include "core/Activity.h"

class HomeScreen final : public Activity {
  int index = 0;
  int itemCount = 0;

 public:
  HomeScreen(Gfx& gfx, MappedInput& input) : Activity("Home", gfx, input) {}
  void onEnter() override;
  void loop() override;
  void render() override;
  bool isHome() const override { return true; }
};
