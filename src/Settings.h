// Runtime settings, editable in the web interface and stored in LittleFS.
// Values from secrets.h / config.h are only the defaults for the first start.
#pragma once

#include <Arduino.h>
#include <FS.h>

struct Settings
{
  String wifiSsid;
  String wifiPassword;
  String awqatEmail;
  String awqatPassword;
  String cityId;
  String timeZone;
  String devicePassword;   // setup access point, web interface and OTA

  void setDefaults();
  bool load(fs::FS& fs);
  bool save(fs::FS& fs) const;

  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasAwqatAccount() const { return awqatEmail.length() > 0 && awqatPassword.length() > 0; }
  bool hasCity() const { return cityId.length() > 0; }
};

// City ids are numeric
bool isValidCityId(const String& id);
