#pragma once

#include "core/Screen.h"

class MessageScreen final : public Screen {
  char text[96]{};

 public:
  MessageScreen(Gfx& gfx, MappedInput& input, const char* message);
  void loop() override;
  void render() override;
};
