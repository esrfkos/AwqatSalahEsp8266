#include "Settings.h"

#include <ArduinoJson.h>

#include "../config.h"
#if __has_include("../secrets.h")
  #include "../secrets.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef AWQAT_EMAIL
  #define AWQAT_EMAIL ""
#endif
#ifndef AWQAT_PASSWORD
  #define AWQAT_PASSWORD ""
#endif
#ifndef DEVICE_PASSWORD
  #define DEVICE_PASSWORD DEFAULT_DEVICE_PASSWORD
#endif

namespace {
const char SETTINGS_FILE[] = "/settings.json";
}

bool isValidCityId(const String& id)
{
  if (id.length() == 0 || id.length() > 10) {
    return false;
  }
  for (size_t i = 0; i < id.length(); i++) {
    if (!isdigit((unsigned char)id[i])) {
      return false;
    }
  }
  return true;
}

void Settings::setDefaults()
{
  wifiSsid       = WIFI_SSID;
  wifiPassword   = WIFI_PASSWORD;
  awqatEmail     = AWQAT_EMAIL;
  awqatPassword  = AWQAT_PASSWORD;
  cityId         = isValidCityId(AWQAT_CITY_ID) ? AWQAT_CITY_ID : "";
  timeZone       = TIME_ZONE;
  devicePassword = DEVICE_PASSWORD;
}

bool Settings::load(fs::FS& fs)
{
  setDefaults();
  File f = fs.open(SETTINGS_FILE, "r");
  if (!f) {
    return false;
  }
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }
  wifiSsid       = doc["wifiSsid"]       | wifiSsid.c_str();
  wifiPassword   = doc["wifiPassword"]   | wifiPassword.c_str();
  awqatEmail     = doc["awqatEmail"]     | awqatEmail.c_str();
  awqatPassword  = doc["awqatPassword"]  | awqatPassword.c_str();
  cityId         = doc["cityId"]         | cityId.c_str();
  timeZone       = doc["timeZone"]       | timeZone.c_str();
  devicePassword = doc["devicePassword"] | devicePassword.c_str();
  return true;
}

bool Settings::save(fs::FS& fs) const
{
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  doc["wifiSsid"]       = wifiSsid;
  doc["wifiPassword"]   = wifiPassword;
  doc["awqatEmail"]     = awqatEmail;
  doc["awqatPassword"]  = awqatPassword;
  doc["cityId"]         = cityId;
  doc["timeZone"]       = timeZone;
  doc["devicePassword"] = devicePassword;

  File f = fs.open(SETTINGS_FILE, "w");
  if (!f) {
    return false;
  }
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}
