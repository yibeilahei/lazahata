#pragma once

#include <HalGPIO.h>

#include <cstdint>

class MappedInput {
 public:
  // PageBack/PageForward are the X3 page keys (up/down).
  enum class Button : uint8_t { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  explicit MappedInput(HalGPIO& gpio) : gpio(gpio) {}

  void update() const { gpio.update(); }

  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const { return gpio.wasAnyPressed(); }
  bool wasAnyReleased() const { return gpio.wasAnyReleased(); }
  unsigned long powerHeldMs() const { return gpio.getPowerButtonHeldTime(); }

  // Call once from setup() after display.begin().
  static void installBusyWaitPoll();

  void resetPendingPageTaps();

  // Net list/page delta from this frame's release edges plus taps drained during a blocking refresh.
  int consumeNavigationDelta();

 private:
  HalGPIO& gpio;
  uint8_t map(Button button) const;

  uint16_t consumePendingForwardTaps();
  uint16_t consumePendingBackTaps();

  // Hook has no instance, so tap counters are static (one MappedInput exists).
  static bool busyWaitPoll(int8_t busyPin, uint8_t busyLevel);
  static uint16_t pendingForwardTaps;
  static uint16_t pendingBackTaps;
};
