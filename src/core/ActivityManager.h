#pragma once

#include <memory>
#include <vector>

#include "core/Activity.h"
#include "core/MappedInput.h"

class Gfx;

class ActivityManager {
  Gfx& gfx;
  MappedInput& input;
  std::unique_ptr<Activity> current;
  std::vector<std::unique_ptr<Activity>> stack;
  std::unique_ptr<Activity> pending;
  enum class Pending { None, Push, Pop, Replace } pendingAction = Pending::None;
  bool dirty = false;

  void applyPending();

 public:
  ActivityManager(Gfx& gfx, MappedInput& input) : gfx(gfx), input(input) { stack.reserve(8); }

  void loop();
  void requestUpdate() { dirty = true; }
  void replace(std::unique_ptr<Activity> activity);
  void push(std::unique_ptr<Activity> activity);
  void pop();
  void goHome();
  void goToReader(const char* path);
  void goToBrowser();
  void goToSettings();
  void showMessage(const char* text);

  bool isReader() const { return current && current->isReader(); }
  Activity* currentActivity() const { return current.get(); }
};

extern ActivityManager activityManager;
