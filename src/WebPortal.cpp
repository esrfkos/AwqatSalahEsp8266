#include "WebPortal.h"

#if defined(ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

namespace {

const byte DNS_PORT = 53;
const char ADMIN_USER[] = "admin";

const char PAGE_HEAD[] PROGMEM = R"html(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AwqatSalah clock</title>
<style>
body{font-family:system-ui,sans-serif;max-width:560px;margin:0 auto;padding:16px;background:#f6f7f9;color:#1f2937}
h1{font-size:1.4em}h2{font-size:1.1em;margin-top:1.6em}
.card{background:#fff;border-radius:10px;padding:14px 16px;margin:12px 0;box-shadow:0 1px 3px #0002}
label{display:block;margin-top:10px;font-weight:600;font-size:.9em}
input,select{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;border:1px solid #cbd5e1;border-radius:6px;font-size:1em}
small{color:#6b7280}button{margin-top:16px;padding:10px 16px;border:0;border-radius:6px;background:#166534;color:#fff;font-size:1em}
button.sec{background:#475569}table{width:100%;border-collapse:collapse}td{padding:3px 0}td:last-child{text-align:right;font-family:monospace}
</style></head><body><h1>AwqatSalah clock</h1>
)html";

const char PAGE_TAIL[] PROGMEM = "</body></html>";

String htmlEscape(const String& s)
{
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  out += F("&amp;");  break;
      case '<':  out += F("&lt;");   break;
      case '>':  out += F("&gt;");   break;
      case '"':  out += F("&quot;"); break;
      case '\'': out += F("&#39;");  break;
      default:   out += c;
    }
  }
  return out;
}

void addField(String& html, const char* label, const char* name, const String& value,
              const char* type = "text", const char* hint = nullptr, const char* list = nullptr)
{
  html += F("<label>");
  html += label;
  html += F("<input name=\"");
  html += name;
  html += F("\" type=\"");
  html += type;
  html += F("\" value=\"");
  html += htmlEscape(value);
  html += '"';
  if (list) {
    html += F(" list=\"");
    html += list;
    html += '"';
  }
  html += F("></label>");
  if (hint) {
    html += F("<small>");
    html += hint;
    html += F("</small>");
  }
}

}  // namespace


void WebPortal::beginSetup(const String& apName, unsigned long timeoutMs)
{
  // scan before the access point starts, scanning later disturbs connected clients
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  networks_ = "";
  for (int i = 0; i < n && i < 20; i++) {
    networks_ += F("<option value=\"");
    networks_ += htmlEscape(WiFi.SSID(i));
    networks_ += F("\">");
  }
  WiFi.scanDelete();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), settings_.devicePassword.length() >= 8 ? settings_.devicePassword.c_str() : nullptr);
  dns_.start(DNS_PORT, "*", WiFi.softAPIP());   // every host name -> the clock (captive portal)

  setupMode_ = true;
  setupStartMs_ = millis();
  setupTimeoutMs_ = timeoutMs;
  registerRoutes();
  Serial.printf("Setup portal: connect to Wi-Fi \"%s\" and open http://%s\n",
                apName.c_str(), WiFi.softAPIP().toString().c_str());
}

void WebPortal::beginStation()
{
  setupMode_ = false;
  registerRoutes();
}

void WebPortal::registerRoutes()
{
  if (started_) {
    return;
  }
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/refresh", HTTP_POST, [this]() { handleRefresh(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  started_ = true;
}

void WebPortal::handle()
{
  if (setupMode_) {
    dns_.processNextRequest();
  }
  if (started_) {
    server_.handleClient();
  }
  if (restartAtMs_ && (long)(millis() - restartAtMs_) >= 0) {
    Serial.println("Restarting with the new settings");
    delay(100);
    ESP.restart();
  }
}

bool WebPortal::setupTimedOut() const
{
  return setupMode_ && setupTimeoutMs_ > 0 && millis() - setupStartMs_ >= setupTimeoutMs_;
}

bool WebPortal::consumeRefreshRequest()
{
  bool r = refreshRequested_;
  refreshRequested_ = false;
  return r;
}

// In setup mode the access point password protects the page,
// in the home network HTTP basic auth with the device password.
bool WebPortal::checkAuth()
{
  if (setupMode_) {
    return true;
  }
  if (server_.authenticate(ADMIN_USER, settings_.devicePassword.c_str())) {
    return true;
  }
  server_.requestAuthentication();
  return false;
}

void WebPortal::sendPage(const String& body, int code)
{
  String html = FPSTR(PAGE_HEAD);
  html += body;
  html += FPSTR(PAGE_TAIL);
  server_.send(code, "text/html; charset=utf-8", html);
}

void WebPortal::handleRoot()
{
  if (!checkAuth()) {
    return;
  }
  String body;
  body.reserve(4096);

  if (!setupMode_ && status_) {
    body += F("<div class=\"card\"><h2>Status</h2>");
    status_(body);
    body += F("<form method=\"post\" action=\"/refresh\"><button class=\"sec\">Download prayer times now</button>"
              "<br><small>Counts against the daily API limit.</small></form></div>");
  }

  body += F("<form method=\"post\" action=\"/save\"><div class=\"card\"><h2>Wi-Fi</h2>");
  body += F("<datalist id=\"nets\">");
  body += networks_;
  body += F("</datalist>");
  addField(body, "Network (SSID)", "wifiSsid", settings_.wifiSsid, "text", nullptr, "nets");
  addField(body, "Wi-Fi password", "wifiPassword", "", "password", "Leave empty to keep the current password.");

  body += F("</div><div class=\"card\"><h2>Awqat Salah API</h2>");
  addField(body, "API e-mail", "awqatEmail", settings_.awqatEmail, "email");
  addField(body, "API password", "awqatPassword", "", "password", "Leave empty to keep the current password.");
  addField(body, "City id", "cityId", settings_.cityId, "text", "Numeric id, see README \"Finding your city id\".");

  body += F("</div><div class=\"card\"><h2>Clock</h2>");
  addField(body, "Time zone (POSIX)", "timeZone", settings_.timeZone, "text",
           "Central Europe: CET-1CEST,M3.5.0,M10.5.0/3");
  addField(body, "Device password", "devicePassword", "", "password",
           "For the setup Wi-Fi, this page (user admin) and OTA updates. Min. 8 characters, empty = keep.");
  body += F("<button>Save and restart</button></div></form>");

  sendPage(body);
}

void WebPortal::handleSave()
{
  if (!checkAuth()) {
    return;
  }
  String city = server_.arg("cityId");
  city.trim();
  String devPw = server_.arg("devicePassword");
  if (city.length() && !isValidCityId(city)) {
    sendPage(F("<div class=\"card\">The city id must be a number. <a href=\"/\">Back</a></div>"), 400);
    return;
  }
  if (devPw.length() && devPw.length() < 8) {
    sendPage(F("<div class=\"card\">The device password needs at least 8 characters. <a href=\"/\">Back</a></div>"), 400);
    return;
  }

  settings_.wifiSsid = server_.arg("wifiSsid");
  settings_.wifiSsid.trim();
  settings_.awqatEmail = server_.arg("awqatEmail");
  settings_.awqatEmail.trim();
  settings_.cityId = city;
  settings_.timeZone = server_.arg("timeZone");
  settings_.timeZone.trim();
  // empty password fields keep the stored value
  if (server_.arg("wifiPassword").length())  settings_.wifiPassword  = server_.arg("wifiPassword");
  if (server_.arg("awqatPassword").length()) settings_.awqatPassword = server_.arg("awqatPassword");
  if (devPw.length())                         settings_.devicePassword = devPw;

  if (!onSave_ || !onSave_()) {
    sendPage(F("<div class=\"card\">Saving failed (file system not available).</div>"), 500);
    return;
  }
  sendPage(F("<div class=\"card\">Saved. The clock restarts and connects to the configured Wi-Fi.</div>"));
  restartAtMs_ = millis() + 1500;
}

void WebPortal::handleRefresh()
{
  if (!checkAuth()) {
    return;
  }
  refreshRequested_ = true;
  server_.sendHeader("Location", "/");
  server_.send(303);
}

void WebPortal::handleNotFound()
{
  if (setupMode_) {
    // captive portal: send every unknown URL to the settings page
    server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server_.send(302);
    return;
  }
  server_.send(404, "text/plain", "Not found");
}
