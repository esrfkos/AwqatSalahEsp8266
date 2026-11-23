// Non-secret settings of the clock. Adjust to your location and wiring.
// The board (ESP32 or ESP8266) is detected automatically from the board selected in the IDE.
#pragma once

// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------

// City id and time zone are only the defaults for the first start,
// afterwards they are changed in the web interface.

// Awqat Salah city id, see "Finding your city id" in the README ("" = set in the web interface).
#define AWQAT_CITY_ID      ""

// POSIX time zone string incl. daylight saving rules.
// Europe/Berlin, Vienna, Zurich, Amsterdam, Brussels, Paris: "CET-1CEST,M3.5.0,M10.5.0/3"
// Europe/London:  "GMT0BST,M3.5.0/1,M10.5.0"
// Europe/Istanbul: "<+03>-3"
// More: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIME_ZONE          "CET-1CEST,M3.5.0,M10.5.0/3"

#define NTP_SERVER_1       "europe.pool.ntp.org"
#define NTP_SERVER_2       "pool.ntp.org"

// ---------------------------------------------------------------------------
// Network setup and OTA
// ---------------------------------------------------------------------------

// Host name in the home network: web interface http://awqatsalah.local, OTA port name
#define DEVICE_NAME              "awqatsalah"

// Setup access point "AwqatSalah-XXXX" (last digits of the MAC address)
#define SETUP_AP_PREFIX          "AwqatSalah-"

// Password of the setup access point, the web interface (user "admin") and OTA
// until a new one is set in the web interface. Min. 8 characters. Change it!
#define DEFAULT_DEVICE_PASSWORD  "awqatsalah"

// With a configured but unreachable Wi-Fi the setup access point stays open this
// long, then the clock restarts and tries the Wi-Fi again.
#define SETUP_PORTAL_TIMEOUT_MS  (5UL * 60UL * 1000UL)

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------

// How many days are downloaded at once: "Monthly", "Weekly" or "Daily".
// The days are stored in flash, so the clock keeps working without internet.
// The API allows only a few requests per day and endpoint, see README.
#define AWQAT_RANGE        "Monthly"

// A new download is started when fewer than this many days (incl. today) are stored.
// One attempt per day at FETCH_HOUR:FETCH_MINUTE (local time).
#define REFRESH_DAYS_AHEAD 5
#define FETCH_HOUR         3
#define FETCH_MINUTE       5

// If today's times are missing, failed downloads are retried after a pause that
// doubles after every failure, from MIN to MAX.
#define FETCH_RETRY_MIN_MS (5UL * 60UL * 1000UL)
#define FETCH_RETRY_MAX_MS (6UL * 60UL * 60UL * 1000UL)

// How long setup() waits for Wi-Fi / NTP before continuing
#define WIFI_TIMEOUT_MS    20000UL
#define NTP_TIMEOUT_MS     10000UL

// Display brightness 0 (dim) .. 7 (bright)
#define DISPLAY_BRIGHTNESS 7

// Content of the small top-left field (positions 0-1)
#define FIELD_OFF          0
#define FIELD_COUNTDOWN    1   // minutes until the next prayer (only when < 100)
#define FIELD_HUMIDITY     2   // relative humidity in %, needs a BME280
#define TOP_LEFT_FIELD     FIELD_COUNTDOWN

// Source of the temperature field (positions 3-4)
#define TEMP_NONE          0
#define TEMP_BME280        1
#define TEMP_DS3231        2

// ---------------------------------------------------------------------------
// Hardware, one block per board. Edit the block of the board you use.
// ---------------------------------------------------------------------------

#if defined(ESP32)

  // Number of TA6932 chips: 2 = prayer times only, 3 = with the clock/date board
  #define DISPLAY_CHIPS        3

  // Chips 1 and 2 (prayer times) share CLK/DIN
  #define PIN_TA6932_STB_1     19
  #define PIN_TA6932_STB_2     17
  #define PIN_TA6932_CLK       18
  #define PIN_TA6932_DIN       5
  // Chip 3 (clock/date board) has its own lines
  #define PIN_TA6932_STB_3     4
  #define PIN_TA6932_CLK_3     0    // strapping pin, must not be pulled low at power-up
  #define PIN_TA6932_DIN_3     2    // strapping pin

  #define PIN_I2C_SDA          22
  #define PIN_I2C_SCL          21

  #define USE_BME280           1
  #define USE_RTC_DS3231       0
  #define TEMPERATURE_SOURCE   TEMP_BME280

#elif defined(ESP8266)

  #define DISPLAY_CHIPS        2

  // NodeMCU / Wemos D1 mini labels in the comments
  #define PIN_TA6932_STB_1     12   // D6
  #define PIN_TA6932_STB_2     14   // D5
  #define PIN_TA6932_CLK       13   // D7
  #define PIN_TA6932_DIN       0    // D3, boot strap pin, must not be pulled low at power-up
  // only used with DISPLAY_CHIPS 3
  #define PIN_TA6932_STB_3     15   // D8
  #define PIN_TA6932_CLK_3     PIN_TA6932_CLK
  #define PIN_TA6932_DIN_3     PIN_TA6932_DIN

  #define PIN_I2C_SDA          4    // D2
  #define PIN_I2C_SCL          5    // D1

  #define USE_BME280           0
  #define USE_RTC_DS3231       1
  #define TEMPERATURE_SOURCE   TEMP_DS3231

#else
  #error "Unsupported board: select an ESP32 or ESP8266 board"
#endif

// I2C address of the BME280 (0x76 or 0x77)
#define BME280_I2C_ADDRESS       0x76

// Segment pattern written to chip 3, digit 14 (indicator next to the clock).
// Taken over from the original sketch; set to 0 to switch it off.
#define CLOCK_INDICATOR_SEGMENTS 0b01011011
