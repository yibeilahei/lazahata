#include "screens/WifiListScreen.h"

#include <Gfx.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "core/UiList.h"
#include "core/fontIds.h"
#include "network/WifiCredentialStore.h"
#include "network/WifiSession.h"
#include "screens/FileTransferScreen.h"
#include "screens/KeyboardScreen.h"

void WifiListScreen::onEnter() {
  Screen::onEnter();
  WifiSession::begin();
  state = State::Scanning;
  networks.clear();
  index = 0;
  window = 0;
  wifiManager.startScan();
  requestUpdate();
}

void WifiListScreen::onExit() {
  Screen::onExit();
  WifiSession::end(gfx);
}

void WifiListScreen::selectNetwork() {
  if (networks.empty()) {
    return;
  }
  const auto& net = networks[static_cast<size_t>(index)];
  pendingSsid = net.ssid;
  enteredPassword.clear();

  if (!net.encrypted) {
    startConnecting(nullptr);
    return;
  }

  if (const auto* cred = wifiCredentials.find(net.ssid.c_str())) {
    enteredPassword = cred->password;
    startConnecting(cred->password);
    return;
  }

  auto keyboard = makeUniqueNoThrow<KeyboardScreen>(gfx, input, *this, "Wi-Fi Password",
                                                    WifiCredentialStore::kPasswordLen - 1, /*passwordMode=*/false);
  if (!keyboard) {
    LOG_ERR("WIFI", "OOM: keyboard");
    return;
  }
  push(std::move(keyboard));
}

void WifiListScreen::startConnecting(const char* password) {
  wifiManager.connect(pendingSsid.c_str(), password);
  state = State::Connecting;
  requestUpdate();
}

void WifiListScreen::onPasswordEntered(const std::string& password) {
  enteredPassword = password;
  startConnecting(password.c_str());
}

void WifiListScreen::onPasswordCancelled() { state = State::NetworkList; }

void WifiListScreen::goToFileTransfer() {
  if (!enteredPassword.empty()) {
    wifiCredentials.addOrUpdate(pendingSsid.c_str(), enteredPassword.c_str());
  }
  auto transfer = makeUniqueNoThrow<FileTransferScreen>(gfx, input, pendingSsid);
  if (!transfer) {
    LOG_ERR("WIFI", "OOM: file transfer");
    return;
  }
  push(std::move(transfer));
}

void WifiListScreen::loop() {
  if (state != State::Failed && input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }

  switch (state) {
    case State::Scanning:
      if (wifiManager.scanComplete(networks)) {
        state = State::NetworkList;
        index = 0;
        window = 0;
        requestUpdate();
      }
      return;
    case State::NetworkList: {
      const int count = static_cast<int>(networks.size());
      if (ui::applyDelta(index, input.consumeNavigationDelta(), count)) {
        requestUpdate();
      } else if (count > 0 && input.wasReleased(MappedInput::Button::Confirm)) {
        selectNetwork();
      }
      return;
    }
    case State::Connecting: {
      const WifiManager::ConnectState result = wifiManager.pollConnect();
      if (result == WifiManager::ConnectState::Connected) {
        goToFileTransfer();
      } else if (result == WifiManager::ConnectState::Failed) {
        state = State::Failed;
        requestUpdate();
      }
      return;
    }
    case State::Failed:
      if (input.wasReleased(MappedInput::Button::Confirm) || input.wasReleased(MappedInput::Button::Back)) {
        state = State::NetworkList;
        requestUpdate();
      }
      return;
  }
}

void WifiListScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 24, "Wi-Fi");

  switch (state) {
    case State::Scanning:
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2, "Scanning...");
      break;
    case State::NetworkList: {
      if (networks.empty()) {
        gfx.drawCenteredText(FONT_UI, gfx.height() / 2, "No networks found");
        break;
      }
      const int rowH = gfx.lineHeight(FONT_UI) + 8;
      const int top = 64;
      const int rows = (gfx.height() - top - 24) / rowH;
      ui::followWindow(window, index, rows);
      const int last = std::min(window + rows, static_cast<int>(networks.size()));
      char label[64];
      for (int i = window; i < last; ++i) {
        const auto& net = networks[static_cast<size_t>(i)];
        snprintf(label, sizeof(label), "%s%s", net.ssid.c_str(), net.encrypted ? "  [locked]" : "");
        ui::drawRow(gfx, top + (i - window) * rowH, rowH, label, i == index);
      }
      break;
    }
    case State::Connecting: {
      char msg[64];
      snprintf(msg, sizeof(msg), "Connecting to %s...", pendingSsid.c_str());
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2, msg);
      break;
    }
    case State::Failed:
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 20, "Connection failed");
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 20, "Press Confirm to try again");
      break;
  }

  presentUi();
}
