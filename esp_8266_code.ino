#include <TA6932.h>
#include <RTClib.h>
#include <time.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

// auth key struct
struct authKey
{
  //expiration;
  //string token;
};

// network settings
// Replace with your network credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PW";

// awqat salah login credentials
const char* awqatSalahLoginEmail = "YOUR_LOGIN_EMAIL";
const char* awqatSalahPassword = "YOUR_PW";

// endpoints for the prayer times
const char* awqatSalahEndpoint = "https://awqatsalah.diyanet.gov.tr/api/PrayerTime/Daily/XXXXX";
const char* awqatSalahDailyPrayerTimesEndPoint = "https://awqatsalah.diyanet.gov.tr/api/PrayerTime/Daily/XXXXXX"; // get your city code and adjust it



// hardware settings
#define RTC_CLOCK          1   // D1
#define RTC_DIN            2   // D2

#define PIN_TA6932_STB_2   14   // D5
#define PIN_TA6932_STB     12   // D6
#define PIN_TA6932_CLK     13   // D7
#define PIN_TA6932_DIN     0    // D3 | 15 D8

// constants for the 7 segment display
#define NONE   0b00000000
#define ZERO   0b01110111 
#define ONE    0b00010010
#define TWO    0b01101011
#define THREE  0b01011011
#define FOUR   0b00011110
#define FIVE   0b01011101
#define SIX    0b01111101
#define SEVEN  0b00010011
#define EIGHT  0b01111111
#define NINE   0b01011111
#define DOT    0b10000000

TA6932 tm(PIN_TA6932_STB, PIN_TA6932_STB_2, PIN_TA6932_CLK, PIN_TA6932_DIN);
RTC_DS3231 rtc;

void setup() {

  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // setup http and wifi clients
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // setup ntp client
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
  timeClient.begin();  
  timeClient.update();
  //---------------------------------------------------------------

  // Check if RTC is connected correctly:
   if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
   // the time will be adjusted once a day via the ntp server
   // set initial time with ntp time
   // rtc.adjust(DateTime(2023,2,10,12,38,0));

  delay(1000);
  tm.begin();
  delay(1000);
  
  //---------------------------------------------------------------
  Serial.println("Started");
}


// TODO: error handling
String getAuthToken(HttpClient http, WiFiClientSecure client, String awqatSalahAuthEndPoint)
{
  DynamicJsonDocument postData(1024);

  client.connect(awqatSalahAuthEndPoint);
  http.begin();
  http.addHeader();
  postData["email"] = awqatSalahLoginEmail;
  postData["password"] = awqatSalahPassword;
  int httpResponseCode = http.POST(postData.as<String>());  
  String payload = http.getString();
  
  return http.getString();
}

// TODO: error handling
DynamicJsonDocument getDailyPrayerTimes(HttpClient http, WiFiClientSecure client, String awqatSalahDailyPrayerTimesEndPoint)
{
  DynamicJsonDocument doc(1024);

  // get daily prayer times for the city and store them
  http.begin(client, awqatSalahEndpoint);
  http.addHeader("Authorization", "Bearer " + accessToken);
  
  // response  
  int responseCode =  http.GET();
  payload = http.getString();
  
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("Failed to parse JSON response");
    // return;
  }
  
  return doc;
}

void loop() {
  
  // setup http and wifi clients
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // setup ntp client
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
  timeClient.begin();  
  timeClient.update();

  // on startup first setup the clock and date
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  Serial.println(timeClient.getFullFormattedTime());


  // then get auth token from awqat salah api, store valid time, since refresh token wont be used dont store or track it

    client.connect("https://awqatsalah.diyanet.gov.tr/Auth/Login", 443);     
    http.begin(client, "https://awqatsalah.diyanet.gov.tr/Auth/Login");      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    DynamicJsonDocument postData(1024);
    postData["email"] = awqatSalahLoginEmail;
    postData["password"] = awqatSalahPassword;
    Serial.println("postData before sending");
    Serial.println(postData.as<String>());

    int httpResponseCode = http.POST(postData.as<String>());
    
    String payload = http.getString();
 
    Serial.println("httpResponseCode");   //Print HTTP return code
    Serial.println(httpResponseCode);   //Print HTTP return code
    Serial.println(payload);    //Print request response payload
    
    DynamicJsonDocument responseData(1024);
    deserializeJson(responseData, http.getString());
    
    String accessToken = responseData["data"]["accessToken"];
    const char* refreshToken = responseData["data"]["refreshToken"];
    Serial.println(accessToken);



  // get daily prayer times for the city and store them
  http.begin(client, awqatSalahEndpoint);
  http.addHeader("Authorization", "Bearer " + accessToken);
  
  // response  
  int responseCode =  http.GET();
  Serial.println("responseCode: ");
  Serial.println(responseCode);

  payload = http.getString();
  Serial.println("payload: ");
  Serial.println(payload);
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("Failed to parse JSON response");
    // return;
  }

    String fajrTime = doc["data"][0]["fajr"];
    String sunriseTime = doc["data"][0]["sunrise"];
    String dhuhrTime = doc["data"][0]["dhuhr"];
    String asrTime = doc["data"][0]["asr"];
    String maghribTime = doc["data"][0]["maghrib"];
    String ishaTime = doc["data"][0]["isha"];

    // Print the prayer times
    Serial.println("fajrTime:" + fajrTime);
    Serial.println("fajrTime removed:" + fajrTime);
    Serial.println("Sunrise: " + sunriseTime);
    Serial.println("Dhuhr: " + dhuhrTime);
    Serial.println("Asr: " + asrTime);
    Serial.println("Maghrib: " + maghribTime);
    Serial.println("Isha: " + ishaTime);
  // every day at 00:10 the prayer times will be updated, therefore new auth key is needed




  // finally display the times on the display
  
  // updateRealTimeOnDisplay();
  // delay(1000);
  imsakRow(returnSevenSegmentValueChar(fajrTime.charAt(0)), returnSevenSegmentValueChar(fajrTime.charAt(1)), returnSevenSegmentValueChar(fajrTime.charAt(3)), returnSevenSegmentValueChar(fajrTime.charAt(4)));
  delay(1000);
  shurukRow(returnSevenSegmentValueChar(sunriseTime.charAt(0)), returnSevenSegmentValueChar(sunriseTime.charAt(1)), returnSevenSegmentValueChar(sunriseTime.charAt(3)), returnSevenSegmentValueChar(sunriseTime.charAt(4)));
  delay(1000);
  dhurRow(returnSevenSegmentValueChar(dhuhrTime.charAt(0)), returnSevenSegmentValueChar(dhuhrTime.charAt(1)), returnSevenSegmentValueChar(dhuhrTime.charAt(3)), returnSevenSegmentValueChar(dhuhrTime.charAt(4)));
  delay(1000);
  asrRow(returnSevenSegmentValueChar(asrTime.charAt(0)), returnSevenSegmentValueChar(asrTime.charAt(1)), returnSevenSegmentValueChar(asrTime.charAt(3)), returnSevenSegmentValueChar(asrTime.charAt(4)));
  delay(1000);
  maghribRow(returnSevenSegmentValueChar(maghribTime.charAt(0)), returnSevenSegmentValueChar(maghribTime.charAt(1)), returnSevenSegmentValueChar(maghribTime.charAt(3)), returnSevenSegmentValueChar(maghribTime.charAt(4)));
  delay(1000);
  ishaRow(returnSevenSegmentValueChar(ishaTime.charAt(0)), returnSevenSegmentValueChar(ishaTime.charAt(1)), returnSevenSegmentValueChar(ishaTime.charAt(3)), returnSevenSegmentValueChar(ishaTime.charAt(4)));
  delay(1000); 
  temperatureRow(ONE, EIGHT); 
  delay(60000);
}

void imsakRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  tm.displayCache[6]  =  first;
  tm.displayCache[7]  =  second + DOT;
  tm.displayCache[8]  =  third + DOT;
  tm.displayCache[9]  =  fourth;
  tm.updateDisplay();
  delay(2000);
}

void shurukRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  tm.displayCache[10]  =  first;
  tm.displayCache[11]  =  second + DOT;
  tm.displayCache[12]  =  third + DOT;
  tm.displayCache[13]  =  fourth;
  tm.updateDisplay();
  delay(2000);
}

void dhurRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  tm.displayCache[14]   =  first;
  tm.displayCache[15]   =  second + DOT;
  tm.displayCache_2[0]  =  third + DOT;
  tm.displayCache_2[1]  =  fourth;
  tm.updateDisplay();
  tm.updateDisplay_2();
  delay(1000);
}

void asrRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  
  tm.displayCache_2[2]  =  first;
  tm.displayCache_2[3]  =  second + DOT;
  tm.displayCache_2[4]  =  third + DOT;
  tm.displayCache_2[5]  =  fourth;
  tm.updateDisplay_2();
  delay(1000);
}

void maghribRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  
  tm.displayCache_2[6]  =  first;
  tm.displayCache_2[7]  =  second + DOT;
  tm.displayCache_2[8]  =  third + DOT;
  tm.displayCache_2[9]  =  fourth;
  tm.updateDisplay_2();
  delay(1000);
}

void ishaRow(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
  
  tm.displayCache_2[10]  =  first;
  tm.displayCache_2[11]  =  second + DOT;
  tm.displayCache_2[12]  =  third + DOT;
  tm.displayCache_2[13]  =  fourth;
  tm.updateDisplay_2();
  delay(1000);
}

void remainingTimeRow(uint8_t tenth, uint8_t ones)
{
  tm.displayCache[0]  =  tenth;
  tm.displayCache[1]  =  ones;
  tm.updateDisplay();
  delay(1000);
}

void temperatureRow(uint8_t tenth, uint8_t ones)
{
  tm.displayCache[3]  =  tenth;
  tm.displayCache[4]  =  ones;
  tm.updateDisplay();
  delay(1000);
}

// dots need to be moved to the regarding row method
void activateDots()
{
  tm.displayCache[7]    =  DOT;
  tm.displayCache[8]    =  DOT;

  tm.displayCache[11]   =  DOT;
  tm.displayCache[12]   =  DOT;

  tm.displayCache[15]   =  DOT;
  tm.displayCache_2[0]  =  DOT;
  
  tm.displayCache_2[3]  =  DOT;
  tm.displayCache_2[4]  =  DOT;

  tm.displayCache_2[7]  =  DOT;
  tm.displayCache_2[8]  =  DOT;

  tm.displayCache_2[11] =  DOT;  
  tm.displayCache_2[12] =  DOT;

  tm.updateDisplay();
  tm.updateDisplay_2();
  delay(1000);
}

void deactivateDots()
{
  tm.displayCache[7]    =  NONE;
  tm.displayCache[8]    =  NONE;

  tm.displayCache[11]   =  NONE;
  tm.displayCache[12]   =  NONE;

  tm.displayCache[15]   =  NONE;
  tm.displayCache_2[0]  =  NONE;
  
  tm.displayCache_2[3]  =  NONE;
  tm.displayCache_2[4]  =  NONE;

  tm.displayCache_2[7]  =  NONE;
  tm.displayCache_2[8]  =  NONE;

  tm.displayCache_2[11] =  NONE;  
  tm.displayCache_2[12] =  NONE;

  tm.updateDisplay();
  tm.updateDisplay_2();
  delay(1000);
}

uint8_t returnSevenSegmentValue(uint8_t input)
{
  uint8_t output;

    switch(input)
  {
    case 0:
      output = ZERO;
    break;
    case 1:
      output = ONE;
    break;
    case 2:
      output = TWO;
    break;
    case 3:
      output = THREE;
    break;
    case 4:
      output = FOUR;
    break;
    case 5:
      output = FIVE;
    break;
    case 6:
      output = SIX;
    break;
    case 7:
      output = SEVEN;
    break;
    case 8:
      output = EIGHT;
    break;
    case 9:
      output = NINE;
    break;
  }
  return output;
}

uint8_t returnSevenSegmentValueChar(char input)
{
  uint8_t output;

    switch(input)
  {
    case '0':
      output = ZERO;
    break;
    case '1':
      output = ONE;
    break;
    case '2':
      output = TWO;
    break;
    case '3':
      output = THREE;
    break;
    case '4':
      output = FOUR;
    break;
    case '5':
      output = FIVE;
    break;
    case '6':
      output = SIX;
    break;
    case '7':
      output = SEVEN;
    break;
    case '8':
      output = EIGHT;
    break;
    case '9':
      output = NINE;
    break;
  }
  return output;
}

void updateRealTimeOnDisplay()
{
  DateTime now = rtc.now();

  uint8_t hours_tens = (now.hour() % 100)/10;
  uint8_t hours_ones = (now.hour() % 10);

  uint8_t minutes_tens = (now.minute() % 100)/10;
  uint8_t minutes_ones = (now.minute() % 10);

  Serial.println("hours tens: ");
  Serial.println(hours_tens);
  Serial.println("hours ones: ");
  Serial.println(hours_ones);

  Serial.println("minutes_tens: ");
  Serial.println(minutes_tens);
  Serial.println("minutes_ones: ");
  Serial.println(minutes_ones);   
  
  // TODO: display on the right row
  maghribRow(returnSevenSegmentValue(hours_tens), 
          returnSevenSegmentValue(hours_ones), 
          returnSevenSegmentValue(minutes_tens), 
          returnSevenSegmentValue(minutes_ones));
  ishaRow(returnSevenSegmentValue(hours_tens), 
          returnSevenSegmentValue(hours_ones), 
          returnSevenSegmentValue(minutes_tens), 
          returnSevenSegmentValue(minutes_ones));          

}

