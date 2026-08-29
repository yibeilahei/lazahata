#pragma once

#include <memory>

#include "core/MappedInput.h"

class Gfx;

class Screen {
  friend class ScreenManager;

 protected:
  const char* name;
  Gfx& gfx;
  MappedInput& input;

 public:
  Screen(const char* name, Gfx& gfx, MappedInput& input) : name(name), gfx(gfx), input(input) {}
  virtual ~Screen() = default;

  virtual void onEnter();
  virtual void onExit();
  // Pop restores the parent without onEnter(); override to refresh state that
  // must change when this screen is uncovered. Call the base so the next
  // presentUi() uses HALF_REFRESH.
  virtual void onResume();
  virtual void loop() {}
  virtual void render() {}
  virtual bool isReader() const { return false; }
  virtual bool blocksSleep() const { return false; }

  void requestUpdate();
  void finish();
  void push(std::unique_ptr<Screen> screen);
  void goHome();
  void goToReader(const char* path);
  void goToBrowser();
  void goToSettings();
  void goToFirmwareUpdate(bool recovery = false);

  // HALF_REFRESH on the first paint after enter/resume, FAST afterwards.
  void presentUi();

 private:
  bool needsCleanRefresh = true;
};
