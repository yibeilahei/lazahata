#include "MappedInput.h"

uint8_t MappedInput::map(const Button button) const {
  switch (button) {
    case Button::Back:
      return HalGPIO::BTN_BACK;
    case Button::Confirm:
      return HalGPIO::BTN_CONFIRM;
    case Button::Left:
      return HalGPIO::BTN_LEFT;
    case Button::Right:
      return HalGPIO::BTN_RIGHT;
    case Button::Up:
      return HalGPIO::BTN_UP;
    case Button::Down:
      return HalGPIO::BTN_DOWN;
    case Button::Power:
      return HalGPIO::BTN_POWER;
    case Button::PageBack:
      return HalGPIO::BTN_UP;
    case Button::PageForward:
      return HalGPIO::BTN_DOWN;
  }
  return HalGPIO::BTN_CONFIRM;
}

bool MappedInput::wasPressed(const Button button) const { return gpio.wasPressed(map(button)); }
bool MappedInput::wasReleased(const Button button) const { return gpio.wasReleased(map(button)); }
bool MappedInput::isPressed(const Button button) const { return gpio.isPressed(map(button)); }
