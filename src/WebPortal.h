// Web interface of the clock.
//
// Setup mode:   own access point with captive portal, opens the settings form
//               automatically on phones/laptops (no Wi-Fi configured or not reachable).
// Station mode: status and settings page in the home network,
//               protected by the device password (user "admin").
#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <functional>

#if defined(ESP32)
  #include <WebServer.h>
  using PortalWebServer = WebServer;
#else
  #include <ESP8266WebServer.h>
  using PortalWebServer = ESP8266WebServer;
#endif

#include "Settings.h"

class WebPortal
{
public:
  using StatusProvider = std::function<void(String& html)>;
  using SaveHandler    = std::function<bool()>;   // persists the settings, true on success

  WebPortal(Settings& settings, SaveHandler onSave, StatusProvider status)
    : settings_(settings), onSave_(onSave), status_(status) {}

  // Starts the access point with captive portal. timeoutMs = 0: no timeout.
  void beginSetup(const String& apName, unsigned long timeoutMs);

  // Starts the settings page in the home network
  void beginStation();

  void handle();

  bool inSetupMode() const { return setupMode_; }
  bool setupTimedOut() const;

  // true once after "download now" was pressed
  bool consumeRefreshRequest();

private:
  void registerRoutes();
  bool checkAuth();
  void handleRoot();
  void handleSave();
  void handleRefresh();
  void handleNotFound();
  void sendPage(const String& body, int code = 200);

  Settings& settings_;
  SaveHandler onSave_;
  StatusProvider status_;
  PortalWebServer server_{80};
  DNSServer dns_;
  bool started_ = false;
  bool setupMode_ = false;
  bool refreshRequested_ = false;
  unsigned long setupStartMs_ = 0;
  unsigned long setupTimeoutMs_ = 0;
  unsigned long restartAtMs_ = 0;
  String networks_;   // <option> list of the networks found before the AP started
};
