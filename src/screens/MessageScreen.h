#pragma once

#include "core/Activity.h"

class MessageScreen final : public Activity {
  char text[96]{};

 public:
  MessageScreen(Gfx& gfx, MappedInput& input, const char* message);
  void loop() override;
  void render() override;
};
