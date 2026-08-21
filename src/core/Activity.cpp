#include "core/Activity.h"

#include "core/ActivityManager.h"

void Activity::requestUpdate() { activityManager.requestUpdate(); }
void Activity::finish() { activityManager.pop(); }
void Activity::push(std::unique_ptr<Activity> activity) { activityManager.push(std::move(activity)); }
void Activity::goHome() { activityManager.goHome(); }
void Activity::goToReader(const char* path) { activityManager.goToReader(path); }
