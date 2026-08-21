#pragma once

#include <HalGPIO.h>

class MappedInput {
 public:
  enum class Button : uint8_t { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  explicit MappedInput(HalGPIO& gpio) : gpio(gpio) {}

  void update() const { gpio.update(); }

  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const { return gpio.wasAnyPressed(); }
  bool wasAnyReleased() const { return gpio.wasAnyReleased(); }
  unsigned long powerHeldMs() const { return gpio.getPowerButtonHeldTime(); }

 private:
  HalGPIO& gpio;
  uint8_t map(Button button) const;
};
