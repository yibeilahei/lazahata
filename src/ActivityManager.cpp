#include "ActivityManager.h"

#include <HalPowerManager.h>
#include <Logging.h>
#include <Memory.h>

#include "Activity.h"
#include "screens/BrowserScreen.h"
#include "screens/HomeScreen.h"
#include "screens/MessageScreen.h"
#include "screens/ReaderScreen.h"
#include "screens/SettingsScreen.h"

void ActivityManager::applyPending() {
  while (pendingAction != Pending::None) {
    if (pendingAction == Pending::Pop) {
      if (current) {
        current->onExit();
        current.reset();
      }
      pendingAction = Pending::None;
      if (stack.empty()) {
        goHome();
        continue;
      }
      current = std::move(stack.back());
      stack.pop_back();
      dirty = true;
      continue;
    }

    if (!pending) {
      pendingAction = Pending::None;
      continue;
    }

    if (pendingAction == Pending::Replace) {
      if (current) {
        current->onExit();
        current.reset();
      }
      stack.clear();
    } else if (pendingAction == Pending::Push) {
      if (current) {
        stack.push_back(std::move(current));
      }
    }

    current = std::move(pending);
    pendingAction = Pending::None;
    if (current) {
      current->onEnter();
    }
    dirty = true;
  }
}

void ActivityManager::loop() {
  applyPending();
  if (current) {
    current->loop();
  }
  applyPending();
  if (dirty && current) {
    HalPowerManager::Lock powerLock;
    current->render();
    dirty = false;
  }
}

void ActivityManager::replace(std::unique_ptr<Activity> activity) {
  pending = std::move(activity);
  pendingAction = Pending::Replace;
}

void ActivityManager::push(std::unique_ptr<Activity> activity) {
  pending = std::move(activity);
  pendingAction = Pending::Push;
}

void ActivityManager::pop() { pendingAction = Pending::Pop; }

void ActivityManager::goHome() {
  auto home = makeUniqueNoThrow<HomeScreen>(gfx, input);
  if (!home) {
    LOG_ERR("ACT", "OOM: home");
    return;
  }
  replace(std::move(home));
}

void ActivityManager::goToReader(const char* path) {
  auto reader = makeUniqueNoThrow<ReaderScreen>(gfx, input, path);
  if (!reader) {
    LOG_ERR("ACT", "OOM: reader");
    return;
  }
  push(std::move(reader));
}

void ActivityManager::goToBrowser() {
  auto browser = makeUniqueNoThrow<BrowserScreen>(gfx, input, "/");
  if (!browser) {
    LOG_ERR("ACT", "OOM: browser");
    return;
  }
  push(std::move(browser));
}

void ActivityManager::goToSettings() {
  auto screen = makeUniqueNoThrow<SettingsScreen>(gfx, input);
  if (!screen) {
    LOG_ERR("ACT", "OOM: settings");
    return;
  }
  push(std::move(screen));
}

void ActivityManager::showMessage(const char* text) {
  auto screen = makeUniqueNoThrow<MessageScreen>(gfx, input, text);
  if (!screen) {
    LOG_ERR("ACT", "OOM: message");
    return;
  }
  replace(std::move(screen));
}
