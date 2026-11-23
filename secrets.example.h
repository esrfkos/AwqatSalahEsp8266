// OPTIONAL. Normally everything is set up in the browser (setup access point).
// If you prefer to compile the credentials in, copy this file to "secrets.h"
// and fill in your values. They are used as defaults until settings are saved
// in the web interface. secrets.h is listed in .gitignore.
#pragma once

// Wi-Fi network the clock connects to (2.4 GHz only)
#define WIFI_SSID          "YOUR_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"

// Awqat Salah API account (request access at https://awqatsalah.diyanet.gov.tr)
#define AWQAT_EMAIL        "YOUR_LOGIN_EMAIL"
#define AWQAT_PASSWORD     "YOUR_AWQAT_PASSWORD"

// Password of the setup access point, web interface (user "admin") and OTA, min. 8 characters
#define DEVICE_PASSWORD    "CHANGE_ME_123"
