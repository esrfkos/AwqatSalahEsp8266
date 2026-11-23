/*
 * AwqatSalahEsp8266
 *
 * Retrofits an Eratec prayer time clock with an ESP32 or ESP8266. Once a day
 * the clock downloads the prayer times from the official Diyanet Awqat Salah
 * API and shows them on the original 7-segment display (TA6932 driver chips).
 * Optionally it shows the current time and date, the temperature/humidity of a
 * BME280 and uses a DS3231 RTC as backup clock.
 *
 * Wi-Fi, API account and city are set up in the browser (setup access point
 * on first start, later http://awqatsalah.local). Firmware updates over Wi-Fi (OTA).
 *
 * Hardware/defaults: config.h
 * Optional defaults for credentials: secrets.h (copy from secrets.example.h)
 * Docs:              README.md
 */

#include "config.h"

#if defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
  #include <esp_sntp.h>
  #include <ESPmDNS.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecure.h>
  #include <coredecls.h>        // settimeofday_cb()
  #include <ESP8266mDNS.h>
#endif

#include <ArduinoOTA.h>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Wire.h>
#include <time.h>
#include <initializer_list>
#include <sys/time.h>

#if USE_RTC_DS3231
  #include <RTClib.h>
#endif
#if USE_BME280
  #include <Adafruit_BME280.h>
#endif

#include "src/TA6932Chain.h"
#include "src/PrayerTable.h"
#include "src/Settings.h"
#include "src/WebPortal.h"

#if TEMPERATURE_SOURCE == TEMP_BME280 && !USE_BME280
  #error "TEMPERATURE_SOURCE is TEMP_BME280 but USE_BME280 is 0"
#endif
#if TEMPERATURE_SOURCE == TEMP_DS3231 && !USE_RTC_DS3231
  #error "TEMPERATURE_SOURCE is TEMP_DS3231 but USE_RTC_DS3231 is 0"
#endif
#if TOP_LEFT_FIELD == FIELD_HUMIDITY && !USE_BME280
  #error "TOP_LEFT_FIELD is FIELD_HUMIDITY but USE_BME280 is 0"
#endif

// ArduinoJson 6 and 7 are both supported
#if ARDUINOJSON_VERSION_MAJOR >= 7
  #define JSON_DOC(name, capacity) JsonDocument name
#else
  #define JSON_DOC(name, capacity) DynamicJsonDocument name(capacity)
#endif

// ---------------------------------------------------------------------------
// Awqat Salah API
// ---------------------------------------------------------------------------

static const char AWQAT_LOGIN_URL[] = "https://awqatsalah.diyanet.gov.tr/Auth/Login";
static const char AWQAT_PRAYER_URL[] = "https://awqatsalah.diyanet.gov.tr/api/PrayerTime/" AWQAT_RANGE "/";   // + city id

// ---------------------------------------------------------------------------
// Prayer times
// ---------------------------------------------------------------------------

// JSON field names in the API response, same order as enum Prayer
static const char* const PRAYER_KEYS[PRAYER_COUNT] = {
  "fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"
};

static const uint16_t MINUTES_PER_DAY = 24 * 60;

// downloaded days, persisted in flash so the clock works without internet
static PrayerTable prayerTable;
static const char PRAYER_FILE[] = "/prayer_times.bin";
static bool fsAvailable = false;

// ---------------------------------------------------------------------------
// Settings, web interface, OTA
// ---------------------------------------------------------------------------

static Settings settings;

static bool saveSettings();
static void statusHtml(String& html);
static WebPortal portal(settings, saveSettings, statusHtml);

static bool otaActive = false;

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
//
// All digits form one buffer, 16 per chip:
//
//   chip 1   0-1    top-left field (countdown / humidity)
//            3-4    temperature
//            6-9    fajr (imsak)          10-13  sunrise (shuruk)
//            14-17  dhuhr (continues on chip 2)
//   chip 2   18-21  asr                   22-25  maghrib
//            26-29  isha
//   chip 3   32-35  year                  36-37  month
//            38-39  day                   40-43  current time HH:MM
//            46     indicator
//
// The colon of a prayer row is the DOT segment of its 2nd and 3rd digit.

// segment bits: 0 top, 1 top-right, 2 top-left, 3 middle, 4 bottom-right, 5 bottom-left, 6 bottom, 7 dot
static const uint8_t SEG_BLANK = 0b00000000;
static const uint8_t SEG_DASH  = 0b00001000;
static const uint8_t SEG_DOT   = 0b10000000;
static const uint8_t SEG_A     = 0b00111111;
static const uint8_t SEG_P     = 0b00101111;

static const uint8_t SEG_DIGITS[10] = {
  0b01110111,  // 0
  0b00010010,  // 1
  0b01101011,  // 2
  0b01011011,  // 3
  0b00011110,  // 4
  0b01011101,  // 5
  0b01111101,  // 6
  0b00010011,  // 7
  0b01111111,  // 8
  0b01011111,  // 9
};

static const uint8_t POS_TOP_LEFT      = 0;
static const uint8_t POS_TEMPERATURE   = 3;
static const uint8_t POS_FIRST_PRAYER  = 6;
static const uint8_t DIGITS_PER_PRAYER = 4;
static const uint8_t POS_YEAR          = 32;
static const uint8_t POS_MONTH         = 36;
static const uint8_t POS_DAY           = 38;
static const uint8_t POS_CLOCK         = 40;
static const uint8_t POS_INDICATOR     = 46;

static const TA6932Pins DISPLAY_PINS[] = {
  { PIN_TA6932_STB_1, PIN_TA6932_CLK,   PIN_TA6932_DIN   },
  { PIN_TA6932_STB_2, PIN_TA6932_CLK,   PIN_TA6932_DIN   },
  { PIN_TA6932_STB_3, PIN_TA6932_CLK_3, PIN_TA6932_DIN_3 },
};
static_assert(DISPLAY_CHIPS >= 2 && DISPLAY_CHIPS <= 3, "DISPLAY_CHIPS must be 2 or 3");

static TA6932Chain display(DISPLAY_PINS, DISPLAY_CHIPS);

// ---------------------------------------------------------------------------
// Sensors / RTC
// ---------------------------------------------------------------------------

#if USE_RTC_DS3231
static RTC_DS3231 rtc;
static bool rtcAvailable = false;
#endif

#if USE_BME280
static Adafruit_BME280 bme;
static bool bmeAvailable = false;
#endif

static volatile bool ntpSynced = false;   // set from the SNTP callback

// ---------------------------------------------------------------------------
// Scheduler state
// ---------------------------------------------------------------------------

static bool fetchAttempted     = false;
static unsigned long lastFetchAttemptMs = 0;
static unsigned long retryDelayMs = FETCH_RETRY_MIN_MS;   // grows after each failure
static int  lastRefreshYday    = -1;      // day of the last planned refresh attempt
static int  lastShownMinute    = -1;


// ===========================================================================
// Display helpers
// ===========================================================================

static uint8_t segDigit(uint8_t value)
{
  return value < 10 ? SEG_DIGITS[value] : SEG_BLANK;
}

// Two-digit number (0-99) or blank if out of range
static void showTwoDigits(uint8_t pos, int value, uint8_t extraSegmentsSecond = 0)
{
  bool valid = value >= 0 && value <= 99;
  display.set(pos,     valid ? segDigit(value / 10) : SEG_BLANK);
  display.set(pos + 1, (valid ? segDigit(value % 10) : SEG_BLANK) | extraSegmentsSecond);
}

// HH:MM row of a prayer time, "--:--" if unknown
static void showPrayerRow(uint8_t prayer, uint16_t minutes)
{
  uint8_t pos = POS_FIRST_PRAYER + prayer * DIGITS_PER_PRAYER;

  if (minutes == NO_TIME) {
    display.set(pos,     SEG_DASH);
    display.set(pos + 1, SEG_DASH | SEG_DOT);
    display.set(pos + 2, SEG_DASH | SEG_DOT);
    display.set(pos + 3, SEG_DASH);
    return;
  }

  uint8_t h = minutes / 60;
  uint8_t m = minutes % 60;
  display.set(pos,     segDigit(h / 10));
  display.set(pos + 1, segDigit(h % 10) | SEG_DOT);
  display.set(pos + 2, segDigit(m / 10) | SEG_DOT);
  display.set(pos + 3, segDigit(m % 10));
}

// Current time and date on the clock board (chip 3), blank if the time is unknown
static void showClock(const struct tm* now)
{
#if DISPLAY_CHIPS >= 3
  if (!now) {
    for (uint8_t pos = POS_YEAR; pos < POS_CLOCK + 4; pos++) {
      display.set(pos, SEG_DASH);
    }
    return;
  }
  int year = now->tm_year + 1900;
  showTwoDigits(POS_YEAR,      year / 100);
  showTwoDigits(POS_YEAR + 2,  year % 100, SEG_DOT);
  showTwoDigits(POS_MONTH,     now->tm_mon + 1, SEG_DOT);
  showTwoDigits(POS_DAY,       now->tm_mday);
  showTwoDigits(POS_CLOCK,     now->tm_hour);
  showTwoDigits(POS_CLOCK + 2, now->tm_min);
  display.set(POS_INDICATOR, CLOCK_INDICATOR_SEGMENTS);
#else
  (void)now;
#endif
}


// ===========================================================================
// Prayer time helpers
// ===========================================================================

// "HH:MM" -> minutes since midnight, NO_TIME on malformed input
static uint16_t parseHHMM(const char* s)
{
  if (!s || !isdigit(s[0]) || !isdigit(s[1]) || s[2] != ':' || !isdigit(s[3]) || !isdigit(s[4])) {
    return NO_TIME;
  }
  uint8_t h = (s[0] - '0') * 10 + (s[1] - '0');
  uint8_t m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) {
    return NO_TIME;
  }
  return h * 60 + m;
}

static uint8_t twoDigits(const char* s)
{
  return (s[0] - '0') * 10 + (s[1] - '0');
}

static bool allDigits(const char* s, std::initializer_list<uint8_t> positions)
{
  for (uint8_t p : positions) {
    if (!isdigit((unsigned char)s[p])) {
      return false;
    }
  }
  return true;
}

// Date of an API entry: "dd.MM.yyyy" (gregorianDateShort) or "yyyy-MM-dd..." (ISO 8601).
// Returns false on malformed input.
static bool parseDate(const char* s, uint16_t& year, uint8_t& month, uint8_t& day)
{
  if (!s || strlen(s) < 10) {
    return false;
  }
  if (s[2] == '.' && s[5] == '.' && allDigits(s, {0, 1, 3, 4, 6, 7, 8, 9})) {
    day   = twoDigits(s);
    month = twoDigits(s + 3);
    year  = twoDigits(s + 6) * 100 + twoDigits(s + 8);
  } else if (s[4] == '-' && s[7] == '-' && allDigits(s, {0, 1, 2, 3, 5, 6, 8, 9})) {
    year  = twoDigits(s) * 100 + twoDigits(s + 2);
    month = twoDigits(s + 5);
    day   = twoDigits(s + 8);
  } else {
    return false;
  }
  return year >= 2000 && year < 2100 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

// One element of the API's "data" array -> DayTimes. Returns false if incomplete.
static bool parseDay(JsonObjectConst entry, DayTimes& out)
{
  if (!parseDate(entry["gregorianDateShort"].as<const char*>(), out.year, out.month, out.day) &&
      !parseDate(entry["gregorianDateLongIso8601"].as<const char*>(), out.year, out.month, out.day)) {
    return false;
  }
  for (uint8_t i = 0; i < PRAYER_COUNT; i++) {
    out.minutes[i] = parseHHMM(entry[PRAYER_KEYS[i]].as<const char*>());
    if (out.minutes[i] == NO_TIME) {
      return false;
    }
  }
  return true;
}

static uint32_t dateKeyOf(const struct tm& t)
{
  return DayTimes::dateKey(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

#if TOP_LEFT_FIELD == FIELD_COUNTDOWN
// Date key of the day after t (mktime handles month/year ends and DST)
static uint32_t nextDateKey(const struct tm& t)
{
  struct tm next = t;
  next.tm_mday += 1;
  next.tm_hour = 12;
  next.tm_isdst = -1;
  mktime(&next);
  return dateKeyOf(next);
}

// Minutes until the next prayer (incl. sunrise), -1 if unknown
static int minutesUntilNextPrayer(const DayTimes* today, const DayTimes* tomorrow, int nowMinutes)
{
  if (!today) {
    return -1;
  }
  for (uint8_t i = 0; i < PRAYER_COUNT; i++) {
    if (today->minutes[i] > nowMinutes) {
      return today->minutes[i] - nowMinutes;
    }
  }
  // after isha: tomorrow's fajr, approximated with today's if not downloaded
  uint16_t fajr = tomorrow ? tomorrow->minutes[FAJR] : today->minutes[FAJR];
  return MINUTES_PER_DAY - nowMinutes + fajr;
}
#endif


// ===========================================================================
// Sensors
// ===========================================================================

static void setupSensors()
{
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

#if USE_RTC_DS3231
  rtcAvailable = rtc.begin();
  if (!rtcAvailable) {
    Serial.println("WARNING: DS3231 not found, running without RTC");
  }
#endif

#if USE_BME280
  bmeAvailable = bme.begin(BME280_I2C_ADDRESS, &Wire);
  if (!bmeAvailable) {
    Serial.printf("WARNING: BME280 not found at 0x%02X (sensor id 0x%02X)\n", BME280_I2C_ADDRESS, bme.sensorID());
  }
#endif
}

// Rounded value or -1 if not available
static int readTemperature()
{
  float t = NAN;
#if TEMPERATURE_SOURCE == TEMP_BME280
  if (bmeAvailable) t = bme.readTemperature();
#elif TEMPERATURE_SOURCE == TEMP_DS3231
  if (rtcAvailable) t = rtc.getTemperature();
#endif
  return isnan(t) ? -1 : (int)lroundf(t);
}

#if TOP_LEFT_FIELD == FIELD_HUMIDITY
static int readHumidity()
{
  float h = bmeAvailable ? bme.readHumidity() : NAN;
  return isnan(h) ? -1 : (int)lroundf(h);
}
#endif


// ===========================================================================
// Time keeping
// ===========================================================================

static bool timeIsValid()
{
  return time(nullptr) > 1700000000;  // any date after Nov 2023
}

#if defined(ESP32)
static void onNtpSync(struct timeval*)
{
  ntpSynced = true;
}
#endif

#if USE_RTC_DS3231
// Copies the RTC time (stored as local time) into the system clock
static void systemTimeFromRtc()
{
  DateTime dt = rtc.now();
  struct tm t = {};
  t.tm_year  = dt.year() - 1900;
  t.tm_mon   = dt.month() - 1;
  t.tm_mday  = dt.day();
  t.tm_hour  = dt.hour();
  t.tm_min   = dt.minute();
  t.tm_sec   = dt.second();
  t.tm_isdst = -1;                    // let mktime apply the TZ rules

  struct timeval tv = { mktime(&t), 0 };
  settimeofday(&tv, nullptr);
  Serial.printf("System time set from RTC: %04d-%02d-%02d %02d:%02d\n",
                dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute());
}

// Writes the current local system time into the RTC
static void rtcFromSystemTime()
{
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
  Serial.println("RTC adjusted from NTP");
}
#endif

static void setupTime()
{
  // The callback runs after every NTP sync (initially and then periodically).
  // It only sets a flag, the I2C access happens in loop().
#if defined(ESP32)
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(settings.timeZone.c_str(), NTP_SERVER_1, NTP_SERVER_2);
#else
  settimeofday_cb([](bool fromSntp) {
    if (fromSntp) {
      ntpSynced = true;
    }
  });
  configTime(settings.timeZone.c_str(), NTP_SERVER_1, NTP_SERVER_2);
#endif

  if (WiFi.status() == WL_CONNECTED) {
    unsigned long start = millis();
    while (!ntpSynced && millis() - start < NTP_TIMEOUT_MS) {
      delay(100);
    }
  }

#if USE_RTC_DS3231
  if (!ntpSynced && rtcAvailable && !rtc.lostPower()) {
    systemTimeFromRtc();
  }
#endif

  if (!timeIsValid()) {
    Serial.println("WARNING: no valid time yet, waiting for NTP");
  }
}


// ===========================================================================
// Network
// ===========================================================================

// Connects to the configured network, false if none is configured or it is not reachable
static bool setupWifi()
{
  if (!settings.hasWifi()) {
    Serial.println("No Wi-Fi configured");
    return false;
  }
  WiFi.persistent(false);       // the settings live in LittleFS, not in the SDK config
#if defined(ESP32)
  WiFi.setHostname(DEVICE_NAME);  // must be set before the mode on ESP32
#else
  WiFi.hostname(DEVICE_NAME);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());

  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Wi-Fi \"%s\" not reachable\n", settings.wifiSsid.c_str());
    return false;
  }
  Serial.printf("Connected, IP %s, http://%s.local\n", WiFi.localIP().toString().c_str(), DEVICE_NAME);
  return true;
}

static void setupOta()
{
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(settings.devicePassword.c_str());
  ArduinoOTA.onStart([]() {
    otaActive = true;
    Serial.println("OTA update started");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int lastPercent = -1;
    int percent = total ? (int)((uint64_t)done * 99 / total) : 0;
    if (percent != lastPercent) {
      lastPercent = percent;
      showTwoDigits(POS_TOP_LEFT, percent);   // progress in the top-left field
      display.flush();
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished, restarting");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    otaActive = false;
    lastShownMinute = -1;   // restore the normal display
    Serial.printf("OTA error %u\n", (unsigned)error);
  });
  ArduinoOTA.begin();       // also announces DEVICE_NAME.local via mDNS
  MDNS.addService("http", "tcp", 80);
}

// Logs in and returns the access token (empty string on failure).
// The token is only valid for a short time, so a new one is requested for every download.
static String awqatLogin(WiFiClientSecure& client)
{
  HTTPClient http;
  http.useHTTP10(true);   // no chunked encoding -> response can be streamed into the JSON parser
  if (!http.begin(client, AWQAT_LOGIN_URL)) {
    Serial.println("Login: begin() failed");
    return String();
  }
  http.addHeader("Content-Type", "application/json");

  JSON_DOC(body, 256);
  body["email"]    = settings.awqatEmail;
  body["password"] = settings.awqatPassword;
  String json;
  serializeJson(body, json);

  int code = http.POST(json);
  if (code != HTTP_CODE_OK) {
    Serial.printf("Login failed: HTTP %d %s\n", code, HTTPClient::errorToString(code).c_str());
    http.end();
    return String();
  }

  JSON_DOC(filter, 64);
  filter["data"]["accessToken"] = true;

  JSON_DOC(response, 1536);
  DeserializationError err = deserializeJson(response, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Login: invalid JSON (%s)\n", err.c_str());
    return String();
  }

  return response["data"]["accessToken"] | "";
}

// Downloads several days (AWQAT_RANGE) and merges them into prayerTable.
// Returns the number of valid days received, 0 on failure.
static uint8_t fetchPrayerTimes(uint32_t todayKey)
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Fetch skipped: no Wi-Fi");
    return 0;
  }
  if (!settings.hasAwqatAccount() || !settings.hasCity()) {
    Serial.println("Fetch skipped: API account or city id missing, see web interface");
    return 0;
  }

  WiFiClientSecure client;
  client.setInsecure();   // the server certificate is not verified, see README

  String token = awqatLogin(client);
  if (token.length() == 0) {
    return 0;
  }

  HTTPClient http;
  http.useHTTP10(true);
  String url = String(AWQAT_PRAYER_URL) + settings.cityId;
  if (!http.begin(client, url)) {
    Serial.println("Prayer times: begin() failed");
    return 0;
  }
  http.addHeader("Authorization", "Bearer " + token);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Prayer times failed: HTTP %d %s\n", code, HTTPClient::errorToString(code).c_str());
    http.end();
    return 0;
  }

  // The "data" array is parsed one element at a time, so a monthly response
  // never has to fit into RAM as a whole.
  Stream& stream = http.getStream();
  if (!stream.find("\"data\"") || !stream.find("[")) {
    Serial.println("Prayer times: no data array in the response");
    http.end();
    return 0;
  }

  JSON_DOC(filter, 256);
  for (const char* key : PRAYER_KEYS) {
    filter[key] = true;
  }
  filter["gregorianDateShort"] = true;
  filter["gregorianDateLongIso8601"] = true;

  static DayTimes received[PrayerTable::CAPACITY];   // static: keep it off the stack
  uint8_t n = 0;
  do {
    JSON_DOC(entry, 512);
    DeserializationError err = deserializeJson(entry, stream, DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("Prayer times: invalid JSON (%s)\n", err.c_str());
      break;
    }
    if (n < PrayerTable::CAPACITY && parseDay(entry.as<JsonObjectConst>(), received[n])) {
      n++;
    }
  } while (stream.findUntil(",", "]"));
  http.end();

  if (n == 0) {
    Serial.println("Prayer times: response contains no valid days");
    return 0;
  }

  prayerTable.merge(received, n, todayKey);
  Serial.printf("Received %u days (%02u.%02u. - %02u.%02u.), %u days stored\n", n,
                received[0].day, received[0].month, received[n - 1].day, received[n - 1].month,
                prayerTable.size());

  if (fsAvailable && !prayerTable.save(LittleFS, PRAYER_FILE, settings.cityId.c_str())) {
    Serial.println("WARNING: could not save the prayer times to flash");
  }
  return n;
}


// ===========================================================================
// Rendering
// ===========================================================================

// now: current local time, nullptr if unknown
static void renderDisplay(const struct tm* now)
{
  const DayTimes* today = now ? prayerTable.find(dateKeyOf(*now)) : nullptr;
  for (uint8_t i = 0; i < PRAYER_COUNT; i++) {
    showPrayerRow(i, today ? today->minutes[i] : NO_TIME);
  }

  if (portal.inSetupMode()) {
    display.set(POS_TOP_LEFT, SEG_A);
    display.set(POS_TOP_LEFT + 1, SEG_P);
  } else {
#if TOP_LEFT_FIELD == FIELD_COUNTDOWN
  const DayTimes* tomorrow = now ? prayerTable.find(nextDateKey(*now)) : nullptr;
  showTwoDigits(POS_TOP_LEFT, now ? minutesUntilNextPrayer(today, tomorrow, now->tm_hour * 60 + now->tm_min) : -1);
#elif TOP_LEFT_FIELD == FIELD_HUMIDITY
  showTwoDigits(POS_TOP_LEFT, readHumidity());
#endif
  }

#if TEMPERATURE_SOURCE != TEMP_NONE
  showTwoDigits(POS_TEMPERATURE, readTemperature());
#endif

  showClock(now);
  display.flush();
}


// ===========================================================================
// Storage
// ===========================================================================

static void setupStorage()
{
  settings.setDefaults();
#if defined(ESP32)
  fsAvailable = LittleFS.begin(true);   // format on first use
#else
  fsAvailable = LittleFS.begin();       // formats automatically on first use
#endif
  if (!fsAvailable) {
    Serial.println("WARNING: LittleFS not available, settings and prayer times are not kept across restarts");
    return;
  }
  if (settings.load(LittleFS)) {
    Serial.println("Settings loaded");
  }
  if (prayerTable.load(LittleFS, PRAYER_FILE, settings.cityId.c_str())) {
    Serial.printf("Loaded %u days from flash\n", prayerTable.size());
  }
}

static bool saveSettings()
{
  return fsAvailable && settings.save(LittleFS);
}

// Status block of the web interface
static void statusHtml(String& html)
{
  html += F("<table>");
  auto row = [&html](const char* label, const String& value) {
    html += F("<tr><td>");
    html += label;
    html += F("</td><td>");
    html += value;
    html += F("</td></tr>");
  };
  char buf[48];

  time_t t = time(nullptr);
  struct tm now;
  localtime_r(&t, &now);
  if (timeIsValid()) {
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", &now);
    row("Time", buf);
    const DayTimes* today = prayerTable.find(dateKeyOf(now));
    static const char* const NAMES[PRAYER_COUNT] = { "Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha" };
    for (uint8_t i = 0; i < PRAYER_COUNT; i++) {
      if (today) {
        snprintf(buf, sizeof(buf), "%02u:%02u", today->minutes[i] / 60, today->minutes[i] % 60);
      } else {
        strcpy(buf, "--:--");
      }
      row(NAMES[i], buf);
    }
    snprintf(buf, sizeof(buf), "%u", prayerTable.daysFrom(dateKeyOf(now)));
    row("Stored days ahead", buf);
  } else {
    row("Time", "unknown");
  }
  if (!settings.hasAwqatAccount() || !settings.hasCity()) {
    row("API", "<b>account or city id missing</b>");
  }
  snprintf(buf, sizeof(buf), "%s, %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  row("Wi-Fi", buf);
  snprintf(buf, sizeof(buf), "%lu h", millis() / 3600000UL);
  row("Uptime", buf);
  snprintf(buf, sizeof(buf), "%u bytes", (unsigned)ESP.getFreeHeap());
  row("Free heap", buf);
  row("Firmware", __DATE__ " " __TIME__);
  html += F("</table>");
}


// ===========================================================================
// Arduino entry points
// ===========================================================================

void setup()
{
  Serial.begin(115200);
  Serial.println("\nAwqatSalahEsp8266 starting");

  delay(500);             // give the display drivers time to power up
  display.begin(DISPLAY_BRIGHTNESS);
  renderDisplay(nullptr); // "--:--" until the time is known

  setupStorage();
  setupSensors();

  if (setupWifi()) {
    portal.beginStation();
    setupOta();
  } else {
    // no or unreachable network: open the setup access point. With a configured
    // network the clock restarts after the timeout and tries again.
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    portal.beginSetup(String(SETUP_AP_PREFIX) + mac.substring(8),
                      settings.hasWifi() ? SETUP_PORTAL_TIMEOUT_MS : 0);
    renderDisplay(nullptr);   // shows "AP"
  }
  setupTime();

  Serial.println("Started");
}

void loop()
{
  portal.handle();
  if (portal.inSetupMode()) {
    if (portal.setupTimedOut()) {
      Serial.println("Setup portal timeout, retrying the configured Wi-Fi");
      ESP.restart();
    }
  } else {
    ArduinoOTA.handle();
  }
  if (otaActive) {
    return;                 // keep the progress display, no downloads during an update
  }

#if USE_RTC_DS3231
  // after an NTP sync keep the RTC in step (flag set in the SNTP callback)
  if (ntpSynced) {
    ntpSynced = false;
    if (rtcAvailable) {
      rtcFromSystemTime();
    }
  }
#endif

  if (!timeIsValid()) {
    delay(10);              // keep the web interface responsive
    return;
  }

  time_t nowEpoch = time(nullptr);
  struct tm now;
  localtime_r(&nowEpoch, &now);

  // Download plan:
  //  - today missing (first start, empty flash, data used up): download now,
  //    retry after failures with growing pauses (FETCH_RETRY_MIN_MS .. MAX_MS)
  //  - fewer than REFRESH_DAYS_AHEAD days left: one attempt per day after
  //    FETCH_HOUR:FETCH_MINUTE, the stored days bridge failed attempts
  // This keeps the number of API requests far below the daily limit.
  uint32_t todayKey = dateKeyOf(now);
  bool haveToday = prayerTable.find(todayKey) != nullptr;
  bool afterFetchTime = now.tm_hour * 60 + now.tm_min >= FETCH_HOUR * 60 + FETCH_MINUTE;

  bool canFetch = !portal.inSetupMode() && settings.hasAwqatAccount() && settings.hasCity();
  bool manual   = portal.consumeRefreshRequest();
  bool urgent   = canFetch && !haveToday && (!fetchAttempted || millis() - lastFetchAttemptMs >= retryDelayMs);
  bool refresh  = canFetch && (manual || (haveToday && afterFetchTime && now.tm_yday != lastRefreshYday &&
                                          prayerTable.daysFrom(todayKey) < REFRESH_DAYS_AHEAD));

  if (urgent || refresh) {
    fetchAttempted = true;
    lastFetchAttemptMs = millis();
    if (refresh && !manual) {
      lastRefreshYday = now.tm_yday;
    }
    Serial.println(urgent ? "Fetching prayer times (today missing)..." : "Refreshing prayer times...");

    bool ok = fetchPrayerTimes(todayKey) > 0 && prayerTable.find(todayKey) != nullptr;
    if (ok) {
      retryDelayMs = FETCH_RETRY_MIN_MS;
      lastShownMinute = -1;   // force redraw
    } else if (urgent) {
      retryDelayMs = min(retryDelayMs * 2, (unsigned long)FETCH_RETRY_MAX_MS);
      Serial.printf("Next attempt in %lu min\n", retryDelayMs / 60000UL);
    }
  }

  // redraw once per minute (only changed chips are sent)
  if (now.tm_min != lastShownMinute) {
    lastShownMinute = now.tm_min;
    renderDisplay(&now);
  }

  delay(10);
}
