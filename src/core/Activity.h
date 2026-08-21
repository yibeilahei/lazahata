#pragma once

#include <Logging.h>

#include <memory>

#include "core/MappedInput.h"

class Gfx;

class Activity {
  friend class ActivityManager;

 protected:
  const char* name;
  Gfx& gfx;
  MappedInput& input;

 public:
  Activity(const char* name, Gfx& gfx, MappedInput& input) : name(name), gfx(gfx), input(input) {}
  virtual ~Activity() = default;

  virtual void onEnter() { LOG_DBG("ACT", "Enter %s", name); }
  virtual void onExit() { LOG_DBG("ACT", "Exit %s", name); }
  virtual void loop() {}
  virtual void render() {}
  virtual bool isReader() const { return false; }
  virtual bool isHome() const { return false; }
  virtual bool blocksSleep() const { return false; }

  void requestUpdate();
  void finish();
  void push(std::unique_ptr<Activity> activity);
  void goHome();
  void goToReader(const char* path);
};
