/* *****************************************************************
 * 
 * Rubato (tempo rubato: "stolen time" - time given back to you)
 * 
 * *****************************************************************/
#define Version "V1.1.1"  // OTA compares this against the ver field; keep bumping every release
/* *****************************************************************
 *  Libraries and headers
 * *****************************************************************/
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <sys/time.h>  // settimeofday: feed the SDK system clock (TLS cert-date checks read time(nullptr), NOT TimeLib)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Updater.h>  // ESP8266 core 2.6.3 name (3.x renamed it to Update.h)
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>
#include "number.h"
#include "weathernum.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


/* *****************************************************************
 *  Feature switches
 * *****************************************************************/
// WEB config enable flag; when on, smartconfig pairing is disabled
#define WM_EN 1



// MQTT enabled (date band displays MQTT messages)
#define MQTT_EN 1



#if WM_EN
#include <WiFiManager.h>
// WiFiManager parameters
WiFiManager wm;  // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#endif






/* *****************************************************************
 *  Fonts and image assets
 * *****************************************************************/
#include "img/misaka.h"
#include "img/temperature.h"
#include "img/humidity.h"
#include "img/health_icons.h"




/* *****************************************************************
 *  Settings
 * *****************************************************************/

struct config_type {
  char stassid[32];  // WiFi SSID from provisioning (32 bytes max)
  char stapsw[64];   // WiFi password from provisioning (64 bytes max)
};

// -------- Edit credentials here (leave empty when WEB config is on) --------
// With WEB config on these can stay empty: ssid first, password second
config_type wificonf = { { "" }, { "" } };


#define WEATHER_REFRESH_MIN 10  // weather refresh interval (minutes)
int LCD_Rotation = 0;           // LCD rotation
int LCD_BL_PWM = 30;            // backlight 0-100, default 30
String cityCode = "Shanghai";   // weather city (English, shown on screen)
float wLat = 0;                 // IP-located latitude (0 = unknown; used by Open-Meteo)
float wLon = 0;                 // IP-located longitude
//----------------------------------------------------

// LCD objects
TFT_eSPI tft = TFT_eSPI();  // pins are set in the TFT_eSPI library User_Setup.h
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 5  // backlight pin

// Backlight PWM entry point: the sketch keeps its legacy 0-1023 duty scale internally,
// but core 3.x changed the default analogWrite range to 0-255 - rescale here so the
// brightness is identical on both cores. ALL backlight writes must go through this.
void blWrite(int duty1023) {
  int d = map(constrain(duty1023, 0, 1023), 0, 1023, 0, 255);
  analogWrite(LCD_BL_PIN, d);
}

/* *****************************************************************
 *  Palette (RGB565, single source of truth - tune globally here)
 *  System: dark (bg), neutral inks (ink1/2/3/model/text_hi), warm (bridge), signal (done)
 *  Target mix: dark >=70% / neutrals ~20% / warm ~8% / green <=2% (finish flash only)

 *  Note: clock digits and weather/temp-humidity icons are pre-rendered JPEGs, not in this palette
 * *****************************************************************/
#define COL_BG 0x0000       // pure black (0,0,0); (10,10,14) tested gray-ish on panel, dropped
#define COL_INK_1 0xC638    // ink 1: city name (198,198,198) - warm gray tested reddish, dropped
#define COL_INK_2 0xAD55    // ink 2: date (168,168,168) neutral gray
#define COL_INK_3 0x9492    // ink 3: temp/humidity values (146,146,146) neutral gray
#define COL_MODEL 0x8410    // deeper ink: MQTT model name (128,128,128)
#define COL_TEXT_HI 0xF79D  // warm white: message body (245,242,236)
#define COL_BRIDGE 0xFED5   // warm bridge: colon, estimate seconds / breath orb (255,216,168)
#define COL_DONE 0x5F10     // signal green (vivid): done finish hold (88,228,128)
#define COL_GEN 0x961E      // glacier blue: Generating breath (150,195,240), warm/cold contrast with cream Thinking

uint16_t bgColor = COL_BG;

// misc status flags
uint8_t UpdateWeater_en = 0;  // weather update request flag
uint8_t tempUnits = 0;        // 0 = Celsius (default), 1 = Fahrenheit (display conversion only)
int prevTime = 0;             // rolling display flag


// EEPROM addr 140-146: day key + five counters; 147-149 spare; 150-194 device identity
int BL_addr = 1;     // addr 1: backlight
int Ro_addr = 2;     // addr 2: rotation
int UN_addr = 3;     // addr 3: temperature units (0 = Celsius default, 1 = Fahrenheit)
int CC_addr = 10;    // addr 10: city
int wifi_addr = 30;  // addr 30: wifi ssid/psw

time_t prevDisplay = 0;        // last displayed time
unsigned long weaterTime = 0;  // last weather update millis
String SMOD = "";              // serial command state


/*** Component objects ***/
Number dig;
WeatherNum wrat;


uint32_t targetTime = 0;


// ESP8266 web server on port 80
ESP8266WebServer server(80);  // ESP8266 web server on port 80


// NTP server pool, tried in order and rotating on failure (no single point of failure):
// pool.ntp.org for overseas units, ntp.aliyun.com for CN networks, cloudflare anycast as tie-break
static const char* ntpServers[] = { "pool.ntp.org", "ntp.aliyun.com", "time.cloudflare.com" };
static const uint8_t NTP_SERVER_COUNT = sizeof(ntpServers) / sizeof(ntpServers[0]);
static uint8_t ntpServerIdx = 0;  // advances on every failed attempt, so the next try uses the next server
long tzOffsetSec = 8 * 3600;  // local offset in seconds; auto-detected from Open-Meteo utc_offset_seconds (DST-aware), default UTC+8

// UDP setup for NTP
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty = 0;

#if MQTT_EN
// ---------- MQTT broker: EMQX Cloud (managed, TLS-only) ----------
// Fill MQTT_HOST with your Serverless deployment host from emqx.com (free tier).
#define MQTT_HOST "ubaa35f0.ala.cn-shenzhen.emqxsl.cn"
#define MQTT_PORT 8883
// Trust anchor for the MQTT TLS connection: EMQX Cloud's chain root (DigiCert Global Root G2,
// public CA, valid to 2038; deployment dashboard shows CA expiry 2031.11.10). Source of truth:
// rubato/certs/emqxsl-ca.crt (downloaded from the EMQX Cloud deployment page). With this set,
// BearSSL verifies the chain AND the hostname - impersonated brokers are rejected.
// CAVEAT: chain validation checks validity dates against the system clock, so the first connect
// waits for NTP (setup syncs before MQTT; reconnects self-heal once time lands).
static const char MQTT_CA[] PROGMEM =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
  "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
  "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
  "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
  "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
  "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
  "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
  "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
  "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
  "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
  "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
  "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
  "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
  "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
  "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
  "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
  "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
  "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
  "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
  "MrY=\n"
  "-----END CERTIFICATE-----\n";
// Per-device identity lives in EEPROM, provisioned per unit over serial:
//   0x06 <deviceId> <token>  writes it,  0x07  wipes it.
// deviceId doubles as the MQTT username, token is the password; both are
// device-specific - no shared secret ships inside the firmware.
// 150-199: AFTER the wifi config struct (30-125) and health counters (140-145),
// so savewificonfig can never clobber the device identity on boot
#define DEV_ID_ADDR 150    // 150-165: deviceId string, NUL-terminated (max 15 chars)
#define DEV_TOKEN_ADDR 166 // 166-199: token string, NUL-terminated (max 32 chars)
char deviceId[16] = "";      // e.g. RUBATO-A1B2C3, shown on the boot screen
char devToken[33] = "";      // random token from the provisioning ledger
char suggestedId[16] = "";   // MAC-derived RUBATO-<last 6 hex>, filled in loadDeviceInfo()
String mqttStateTopic = "";  // rubato/<deviceId>/state, built in loadDeviceInfo()

WiFiClientSecure* mqttNet = nullptr;  // heap object: OTA deletes it to free the MQTT TLS RAM
BearSSL::X509List* mqttCA = nullptr;  // parsed trust anchor, built once and reused across reconnects
PubSubClient mqttClient;               // bound to *mqttNet in mqttConnect via setClient
String mqttMsg = "";           // latest MQTT message
bool mqttDirty = false;        // fresh message pending repaint
uint32_t mqttLastMsgTime = 0;  // last message millis
uint32_t mqttReconnectT = 0;   // reconnect timer

bool deviceProvisioned() { return deviceId[0] != 0 && devToken[0] != 0; }

// read deviceId/token back from EEPROM; erased flash reads 0xFF, so only
// printable ASCII counts (anything else ends the string)
void loadDeviceInfo() {
  memset(deviceId, 0, sizeof(deviceId));
  for (int i = 0; i < 15; i++) {
    char c = (char)EEPROM.read(DEV_ID_ADDR + i);
    if (c < 0x21 || c > 0x7E) break;
    deviceId[i] = c;
  }
  memset(devToken, 0, sizeof(devToken));
  for (int i = 0; i < 32; i++) {
    char c = (char)EEPROM.read(DEV_TOKEN_ADDR + i);
    if (c < 0x21 || c > 0x7E) break;
    devToken[i] = c;
  }
  mqttStateTopic = String("rubato/") + deviceId + "/state";
  // suggested identity from the chip MAC: hardware-anchored, available even
  // before provisioning - the boot screen shows it so 0x06 input is copy-free
  // MAC-derived identity: strip separators and normalize case first (the
  // format differs across cores: with/without colons, upper/lower), then take
  // the LAST 6 hex digits, e.g. 3c8a1f43216c -> RUBATO-43216c
  String mac = WiFi.macAddress();
  String hex = "";
  for (unsigned int i = 0; i < mac.length(); i++) {
    char c = mac[i];
    if (isxdigit(c)) hex += (char)tolower(c);
  }
  String sug = String("RUBATO-") + (hex.length() >= 6 ? hex.substring(hex.length() - 6) : hex);
  sug.toCharArray(suggestedId, sizeof(suggestedId));
}

// 0x06 <deviceId> <token>: write the identity, commit, reboot into it
bool provisionDevice(String id, String token) {
  id.trim();
  token.trim();
  if (id.length() < 3 || id.length() > 15) {
    Serial.println("[PROV] bad deviceId: 3-15 chars, no spaces (e.g. RUBATO-A1B2C3)");
    return false;
  }
  if (token.length() < 8 || token.length() > 32) {
    Serial.println("[PROV] bad token: 8-32 chars");
    return false;
  }
  for (unsigned int i = 0; i < id.length(); i++) EEPROM.write(DEV_ID_ADDR + i, id[i]);
  EEPROM.write(DEV_ID_ADDR + id.length(), 0);
  for (unsigned int i = 0; i < token.length(); i++) EEPROM.write(DEV_TOKEN_ADDR + i, token[i]);
  EEPROM.write(DEV_TOKEN_ADDR + token.length(), 0);
  EEPROM.commit();
  Serial.print("[PROV] saved deviceId=");
  Serial.print(id);
  Serial.println(", rebooting...");
  delay(200);
  ESP.restart();
  return true;
}

// 0x07: erase the identity (returns/RMA), reboot unprovisioned
void wipeDevice() {
  for (int i = 0; i < (int)sizeof(deviceId); i++) EEPROM.write(DEV_ID_ADDR + i, 0);
  for (int i = 0; i < (int)sizeof(devToken); i++) EEPROM.write(DEV_TOKEN_ADDR + i, 0);
  EEPROM.commit();
  Serial.println("[PROV] device identity wiped, rebooting...");
  delay(200);
  ESP.restart();
}
#endif


// forward declarations
time_t getNtpTime();
void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress& address);
void LCD_reflash(int en);
void savewificonfig();
void readwificonfig();
void deletewificonfig();
void Web_Sever_Init();
void Web_Sever();
void saveCityNametoEEP(String cityname);
String readCityNamefromEEP();
void drawDateArea();
String week();
String monthDay();

/* *****************************************************************
 *  Functions
 * *****************************************************************/
// save city name to EEPROM
void saveCityNametoEEP(String cityname) {
  int len = cityname.length();
  if (len > 19) len = 19;
  EEPROM.write(CC_addr, len);  // write length
  EEPROM.commit();
  delay(5);
  for (int i = 0; i < len; i++) {
    EEPROM.write(CC_addr + 1 + i, (uint8_t)cityname[i]);
    EEPROM.commit();
    delay(5);
  }
}
// read city name from EEPROM (validated; legacy 9-digit codes rejected)
String readCityNamefromEEP() {
  int len = EEPROM.read(CC_addr);
  String s = "";
  if (len > 0 && len < 20) {
    bool valid = true;
    for (int i = 0; i < len; i++) {
      char c = (char)EEPROM.read(CC_addr + 1 + i);
      // letters, digits, space, hyphen and comma only
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == ',')) {
        valid = false;
        break;
      }
      s += c;
    }
    if (!valid) return "";
    // all digits, 5-9 chars: legacy city-code format, invalid
    if (s.length() >= 5 && s.length() <= 9) {
      bool allDigit = true;
      for (int i = 0; i < s.length(); i++) {
        if (!((s[i] >= '0') && (s[i] <= '9'))) {
          allDigit = false;
          break;
        }
      }
      if (allDigit) return "";
    }
  }
  return s;
}

// save wifi ssid/psw to EEPROM
void savewificonfig() {
  // begin write
  uint8_t* p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++) {
    EEPROM.write(i + wifi_addr, *(p + i));  // emulated write into flash
  }
  delay(10);
  EEPROM.commit();  // commit
  delay(10);
}
// erase stored wifi info
void deletewificonfig() {
  config_type deletewifi = { { "" }, { "" } };
  uint8_t* p = (uint8_t*)(&deletewifi);
  for (int i = 0; i < sizeof(deletewifi); i++) {
    EEPROM.write(i + wifi_addr, *(p + i));  // emulated write into flash
  }
  delay(10);
  EEPROM.commit();  // commit
  delay(10);
}

// read wifi ssid/psw from EEPROM
void readwificonfig() {
  uint8_t* p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++) {
    *(p + i) = EEPROM.read(i + wifi_addr);
  }
  Serial.printf("[WIFI] config loaded: ssid='%s'\r\n", wificonf.stassid);  // psw intentionally not logged
}

// TFT JPEG output callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

// progress bar counter
byte loadNum = 6;
void loading(byte delayTime)  // draw boot screen: progress bar, status, id plate
{
  clk.setColorDepth(8);

  clk.createSprite(240, 120);  // full-width window over the lower screen half
  clk.fillSprite(COL_BG);      // fill

  // progress bar
  clk.drawRoundRect(20, 8, 200, 16, 8, 0xFFFF);       // round rect outline
  clk.fillRoundRect(23, 11, loadNum, 10, 5, 0xFFFF);  // filled round rect

  // connection status (font 2: the original typeface and wording)
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_GREEN, COL_BG);
  clk.drawString("Connecting to WiFi......", 120, 40, 2);

  // bottom plate row: device id as provisioned (or the MAC-derived suggestion),
  // version pinned bottom-right; x=108 keeps clearance from the version string
  clk.setTextColor(COL_INK_2, COL_BG);
  clk.drawString(deviceId[0] ? deviceId : suggestedId, 108, 106, 2);
  clk.drawRightString(Version, 236, 106, 2);

  clk.pushSprite(0, 110);  // window position
  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

// product wordmark, centered in the middle of the upper screen half (above the boot sprite)
void drawBootTitle() {
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.drawString("Rubato", 120, 52);
  tft.setTextDatum(TL_DATUM);
}


// serial command handler
void Serial_set() {
  String incomingByte = "";
  if (Serial.available() > 0) {

    while (Serial.available() > 0)  // drain the serial buffer into incomingByte
    {
      incomingByte += char(Serial.read());  // read one char at a time, appended in order
      delay(2);                             // required: gives the UART time to fill the buffer
    }
    incomingByte.trim();  // monitors append \r\n; trim it or command compares fail (0x04/0x05 included)
    if (incomingByte.length() == 0) return;
    if (incomingByte == "done") {  // serial test shortcut: same as MQTT done - the restore channel after a 0x05 demo
      applyMqttMsg("{\"state\":\"done\"}");
      return;
    }
    if (SMOD == "0x01")  // value for 0x01: brightness
    {
      int LCDBL = atoi(incomingByte.c_str());
      if (LCDBL >= 0 && LCDBL <= 100) {
        EEPROM.write(BL_addr, LCDBL);       // write brightness
        EEPROM.commit();               // commit
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr);
        delay(5);
        SMOD = "";
        Serial.printf("Brightness set to: ");
        blWrite(1023 - (LCD_BL_PWM * 10));
        Serial.println(LCD_BL_PWM);
        Serial.println("");
      } else
        Serial.println("Invalid brightness. Please enter a value between 0 and 100.");
    } else if (SMOD == "0x02")  // value for 0x02: city name
    {
      String CityName = incomingByte;
      CityName.trim();
      if (CityName.length() >= 1 && CityName.length() < 20) {
        cityCode = CityName;
        saveCityNametoEEP(cityCode);
        Serial.printf("City set to: ");
        Serial.println(cityCode);
        Serial.println("");
        getCityWeater();  // refresh weather for the new city
        SMOD = "";
      } else
        Serial.println("Invalid city name. Enter a city name in English, e.g. Changsha.");
    } else if (SMOD == "0x03")  // value for 0x03: rotation
    {
      int RoSet = atoi(incomingByte.c_str());
      if (RoSet >= 0 && RoSet <= 3) {
        EEPROM.write(Ro_addr, RoSet);  // write rotation
        EEPROM.commit();               // commit
        SMOD = "";
        // apply rotation and repaint
        tft.setRotation(RoSet);
        tft.fillScreen(COL_BG);
        LCD_reflash(1);  // repaint static frame
        UpdateWeater_en = 1;
        TJpgDec.drawJpg(40, 213, temperature, sizeof(temperature));  // temperature icon (bottom row)
        TJpgDec.drawJpg(130, 213, humidity, sizeof(humidity));       // humidity icon (bottom row, side by side)

        Serial.print("Rotation set to: ");
        Serial.println(RoSet);
      } else {
        Serial.println("Invalid rotation. Please enter a value between 0 and 3.");
      }
    } else {
      SMOD = incomingByte;
      delay(2);
      if (SMOD == "0x01")
        Serial.println("Enter a brightness value between 0 and 100.");
      else if (SMOD == "0x02")
        Serial.println("Enter a city name in English, e.g. Changsha.");
      else if (SMOD == "0x03") {
        Serial.println("Enter a rotation value:");
        Serial.println("0 - USB port facing down");
        Serial.println("1 - USB port facing right");
        Serial.println("2 - USB port facing up");
        Serial.println("3 - USB port facing left");
      } else if (SMOD == "0x04") {
        Serial.println("Resetting WiFi settings...");
        delay(10);
        wm.resetSettings();
        deletewificonfig();
        delay(10);
        Serial.println("WiFi settings reset.");
        SMOD = "";
        ESP.restart();
      } else if (SMOD.startsWith("0x06")) {
        // 0x06 <deviceId> <token>: provision this unit (values from the ledger)
        String args = SMOD.substring(4);
        args.trim();
        int sp = args.indexOf(' ');
        if (sp > 0) {
          provisionDevice(args.substring(0, sp), args.substring(sp + 1));
        } else {
          Serial.println("Usage: 0x06 <deviceId> <token>   e.g. 0x06 RUBATO-A1B2C3 3f9c8e21...");
        }
        SMOD = "";
      } else if (SMOD == "0x07") {
        wipeDevice();  // erase identity and reboot (returns/RMA)
      } else if (SMOD.startsWith("0x05")) {
        int8_t forced = -1;              // -1 = random pick (counts quota)
        String arg = SMOD.substring(4);  // "0x05 1".."0x05 5" = fixed activity, display test only
        arg.trim();
        if (arg.length() > 0) forced = (int8_t)(arg.toInt() - 1);
        healthForceDemo(forced);
        SMOD = "";
      } else if (SMOD == "0x09") {
        healthPrintCounts();
        SMOD = "";
      } else if (SMOD == "0x08") {
        Serial.println("[SYS] reboot on request");
        SMOD = "";
        delay(10);
        ESP.restart();  // clean reboot: nothing wiped, MQTT and clock come back on boot
      } else {
        Serial.println("");
        Serial.println("Please enter a command:");
        Serial.println("Brightness setting      0x01");
        Serial.println("City name setting       0x02");
        Serial.println("Rotation setting        0x03");
        Serial.println("Reset WiFi (reboots)    0x04");
        Serial.println("Health reminder demo    0x05 [1-6]");
        Serial.println("Provision device        0x06 <id> <token>");
        Serial.println("Wipe device identity    0x07");
        Serial.println("Reboot device           0x08");
        Serial.println("Health counts today     0x09");
        Serial.println("");
      }
    }
  }
}

#if MQTT_EN
// ---------- MQTT (EMQX Cloud, per-device identity over TLS) ----------
// connect as deviceId/token; skip entirely when the unit is not provisioned
bool mqttConnect() {
  if (!deviceProvisioned()) {
    Serial.print("[MQTT] not provisioned - this unit's suggested id: ");
    Serial.println(suggestedId);
    return false;
  }
  // BearSSL buffer trim: 4096 rx fits the Let's Encrypt chain, 1024 tx is plenty
  // for our tiny packets; keeps the handshake heap peak around 16KB
  if (!mqttNet) { mqttNet = new WiFiClientSecure(); mqttClient.setClient(*mqttNet); }
  mqttNet->setBufferSizes(4096, 1024);
  if (!mqttCA) {
    Serial.println("[MQTT] parsing pinned CA: DigiCert Global Root G2 (certs/emqxsl-ca.crt)");
    mqttCA = new BearSSL::X509List(MQTT_CA);  // PROGMEM-aware per the core's BearSSL helpers
    Serial.printf("[MQTT] CA anchors parsed: %d\n", mqttCA->getCount());
  }
  mqttNet->setTrustAnchors(mqttCA);  // chain + hostname verified; pre-NTP boot waits for a sane clock

  String clientId = String(deviceId);  // already unique and prefixed (RUBATO-xxxxxx)
  bool ok = mqttClient.connect(clientId.c_str(), deviceId, devToken);
  if (ok) {
    mqttClient.subscribe(mqttStateTopic.c_str());
    Serial.print("[MQTT] connected as ");
    Serial.print(deviceId);
    Serial.print(" -> ");
    Serial.println(mqttStateTopic);
  } else {
    char tlsErr[80];
    mqttNet->getLastSSLError(tlsErr, sizeof(tlsErr));
    Serial.printf("[MQTT] connect failed, rc=%d, now()=%lu, TLS: %s\n",
                  mqttClient.state(), (unsigned long)now(), tlsErr);
  }
  return ok;
}

// MQTT subscribe callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  mqttMsg = "";
  for (unsigned int i = 0; i < length; i++) mqttMsg += (char)payload[i];
  mqttLastMsgTime = millis();
  mqttDirty = true;
}

// ---------- Message band state machine: date, intro caption, breath orb ----------
// Flow: estimate/thinking/generating shows the caption (fade in, hold, fade out; model name, future: logos);
//   then the orb starts breathing (Thinking cream slow / Generating blue fast, 600ms style crossfade);
//   done: orb turns green, flashes and finishes, back to date. Plain-text messages are ignored by convention.
enum BandPhase : uint8_t { BAND_IDLE = 0,
                           BAND_INTRO = 1,
                           BAND_WORKING = 2,
                           BAND_FADEOUT = 3,
                           BAND_HEALTH = 4 };

BandPhase bandPhase = BAND_IDLE;
bool bandGen = false;      // target phase: false = Thinking / true = Generating
String introModel = "";    // intro caption model name (future: per-LLM logos)
float workEstSec = -1.0f;  // latest estimate seconds; over threshold arms the reminder (cleared on new task / done)
uint32_t introStartMs = 0;
uint32_t workStartMs = 0;  // WORKING entry time (reserved timing base)
uint32_t fadeStartMs = 0;

// two breath styles: period ms; amp; riseFrac = inhale share (<0.5 = longer exhale, looser); lit = main color
struct BreathStyle {
  float periodMs;
  float amp;
  float riseFrac;
  uint16_t lit;
};
const BreathStyle BS_THINK = { 2600.0f, 0.78f, 0.40f, COL_BRIDGE };  // Thinking: cream, slow, soft, long exhale
const BreathStyle BS_GEN   = { 1400.0f, 1.00f, 0.50f, COL_GEN   };   // Generating: glacier blue, fast, bright (warm/cold contrast)
BreathStyle bPrev = BS_THINK;
BreathStyle bCur = BS_THINK;
uint32_t bXfadeMs = 0;   // style crossfade start
float bPhaseAcc = 0.0f;  // phase accumulator (tempo changes stay beat-safe)
uint32_t bLastMs = 0;

const uint32_t INTRO_IN_MS = 400;        // caption fade-in
const uint32_t INTRO_HOLD_MS = 900;      // caption hold
const uint32_t INTRO_OUT_MS = 500;       // caption fade-out
const uint32_t PHASE_XFADE_MS = 600;     // Thinking <-> Generating style crossfade
const uint32_t BAND_FADE_MS = 2600;      // done finish total: green 0.5s -> hold 1.45s -> die 0.65s
const uint32_t DATE_IN_MS = 600;         // date fade-in after done (same language as caption, no abrupt jump)
const uint8_t ORB_W = 72, ORB_H = 60;    // orb canvas (16-bit) - half the memory of a full-width one
const uint8_t ORB_CX = 36, ORB_CY = 29;  // canvas center (pushed to screen x=84,y=150 -> screen center 120,179)
const uint8_t ORB_PUSH_X = 84;

BreathStyle bEff = BS_THINK;    // effective style output (kept global so no custom type appears in signatures,
                                // otherwise core 2.6.3 auto-prototypes land before the struct and break the build)
uint32_t lastDateAreaDraw = 0;  // draw throttle timestamp
uint32_t dateInStartMs = 0;     // date fade-in start (0 = not fading)

TFT_eSprite sprOrb = TFT_eSprite(&tft);  // orb-only 16-bit canvas: allocated on breath entry, freed on return to date
bool sprOrbOn = false;

// wipe the whole message band to background (clears the previous phase)
void wipeBand() {
  clk.setColorDepth(8);
  if (!clk.createSprite(240, 60)) {  // low heap: paint straight to screen
    tft.fillRect(0, 150, 240, 60, bgColor);
    return;
  }
  clk.fillSprite(bgColor);
  clk.pushSprite(0, 150);
  clk.deleteSprite();
}

// orb canvas lifecycle
void orbCanvas(bool allocIt) {
  if (allocIt && !sprOrbOn) {
    sprOrb.setColorDepth(16);
    sprOrb.createSprite(ORB_W, ORB_H);
    sprOrbOn = (sprOrb.getPointer() != nullptr);
  } else if (!allocIt && sprOrbOn) {
    sprOrb.deleteSprite();
    sprOrbOn = false;
  }
}

// enter breath phase: reset clocks, wipe band, ready the canvas
void orbBegin(uint32_t nowMs) {
  workStartMs = nowMs;
  bLastMs = nowMs;
  bPhaseAcc = 0.0f;
  wipeBand();
  orbCanvas(true);
}

// ============ Health reminder (V1.2) ============
// AI activity means not resting: any Estimate >= threshold arms a micro-break - no clock-based
// window. Two health full-screens stay >= 30 min apart (lastHealthMs), daily quota per activity.
#define HEALTH_EST_SEC 30.0f                  // trigger threshold: estimate seconds (estSec >= 30)
#define HEALTH_MIN_GAP_MS (30UL * 60 * 1000)  // min gap between health full-screens: 30 min
#define HEALTH_ACTS 6           // activity count (water/toilet/eyes/neck/kegel/stand)
#define HEALTH_ADDR 140         // EEPROM addr 140-141: day key, 142-147: six counters (deviceId at 150, 148-149 spare)
// two-line reminder copy: natural sentence flow ("Drink One Glass / of Water"); the quantity/duration
// token sits where the sentence puts it, never the same slot every time (uniform = stiff). Each line
// = three segments pre/accent/post drawn white/GOLD/white and centered as a whole; empty = skipped.
const char* const HEALTH_LINES[HEALTH_ACTS][2][3] = {
  { { "Drink", " ONE ", "" },     { "Glass of Water", "", "" } },           // Drink One Glass of Water
  { { "", " TWO ", " Mins" },        { "Bathroom Break", "", "" } },     // A Two-Minute Bathroom Break
  { { "Look Far Away", "", "" },       { "for", " THREE ", " Mins" } },  // Look Far Away for Three Minutes
  { { "Unwind Your Neck", "", "" },    { "for", " THREE ", " Mins" } },  // Unwind Your Neck for Three Minutes
  { { "Do", " TEN ", "" },             { "Slow Kegels", "", "" } },      // Do Ten Slow Kegels
  { { "On Your Feet", "", "" },        { "for", " THIRTY ", " Mins" } }, // On Your Feet for THIRTY Mins
};
String healthPhrase(int8_t a) {  // full sentence for serial logs
  if (a < 0 || a >= HEALTH_ACTS) return "?";
  return String(HEALTH_LINES[a][0][0]) + HEALTH_LINES[a][0][1] + HEALTH_LINES[a][0][2] + " " +
         HEALTH_LINES[a][1][0] + HEALTH_LINES[a][1][1] + HEALTH_LINES[a][1][2];
}
const uint8_t HEALTH_QUOTA[HEALTH_ACTS] = { 4, 2, 2, 2, 2, 1 };
const char* const HEALTH_NAMES[HEALTH_ACTS] = { "water", "toilet", "eyes", "neck", "kegel", "stand" };  // short tags for count dumps
// activity icons (100x100 JPEG on black, generated into img/health_icons.h), same index
const uint8_t* const HEALTH_ICONS[HEALTH_ACTS] = { icon_water_jpg, icon_toilet_jpg, icon_eyes_jpg, icon_neck_jpg, icon_kegel_jpg, icon_stand_jpg };
const uint32_t HEALTH_ICON_LEN[HEALTH_ACTS] = { sizeof(icon_water_jpg), sizeof(icon_toilet_jpg),
                                                sizeof(icon_eyes_jpg), sizeof(icon_neck_jpg), sizeof(icon_kegel_jpg),
                                                sizeof(icon_stand_jpg) };

uint8_t healthCount[HEALTH_ACTS] = { 0, 0, 0, 0, 0, 0 };  // reminders shown today
uint16_t healthDayKey = 0;                // day key: month*100+day
uint32_t lastHealthMs = 0;                // last reminder millis
int8_t healthActivity = -1;               // activity index shown on the health page
bool healthPending = false;               // reminder armed for this task (page not up yet)
bool     healthShownTask  = false;           // this task already showed its reminder (single-shot latch)

// health page stages: 0=BL fade-down (whole screen) 1=dark: clear + draw icon/copy, BL up 2=static hold 3=done: BL down 4=dark restore, BL up -> IDLE
uint8_t healthStage = 0;
uint32_t healthStageStart = 0;
int healthBlFrom = 723;     // backlight duty when entering the page (stage-1 ramp target)
int healthBlCur = 723;      // last written duty (done continues dimming from here)
int healthBlRestore = 723;  // restore target duty for the finish

int blDutyNow() {
  return 1023 - LCD_BL_PWM * 10;
}  // backlight duty: higher = darker (active-low panel)

// latest weather cache (health-page takeover updates the cache only; restore repaints)
int lastWcode = -1;
int lastTemp = 0;
int lastHumi = 0;
bool weatherValid = false;

uint16_t healthDayKeyNow() {
  return (uint16_t)(month() * 100 + day());
}

// serial 0x09: dump today's counters (lives after the health globals; Arduino auto-prototypes it
// so Serial_set() - defined earlier in the file - can call it without forward-declaration games)
void healthPrintCounts() {
  Serial.printf("[HEALTH] day %u - reminders shown today:\n", healthDayKey);
  for (uint8_t i = 0; i < HEALTH_ACTS; i++)
    Serial.printf("  %-7s %u/%u\n", HEALTH_NAMES[i], healthCount[i], HEALTH_QUOTA[i]);
}

void healthResetCounters() {
  for (uint8_t i = 0; i < HEALTH_ACTS; i++) healthCount[i] = 0;
  healthDayKey = healthDayKeyNow();
}

void healthLoad()  // boot loads counters only; day rollover is checked at first trigger eval (NTP synced by then)
{
  healthDayKey = (uint16_t)((EEPROM.read(HEALTH_ADDR) << 8) | EEPROM.read(HEALTH_ADDR + 1));
  bool bad = false;
  for (uint8_t i = 0; i < HEALTH_ACTS; i++) {
    healthCount[i] = EEPROM.read(HEALTH_ADDR + 2 + i);
    if (healthCount[i] > 9) bad = true;  // fresh 0xFF bytes (incl. the new stand slot) reset all counters once
  }
  if (bad) healthResetCounters();
}

void healthSave() {
  EEPROM.write(HEALTH_ADDR, (uint8_t)(healthDayKey >> 8));
  EEPROM.write(HEALTH_ADDR + 1, (uint8_t)(healthDayKey & 0xFF));
  for (uint8_t i = 0; i < HEALTH_ACTS; i++) EEPROM.write(HEALTH_ADDR + 2 + i, healthCount[i]);
  EEPROM.commit();
}

// pick a random activity with quota left; -1 when full
int8_t healthPick() {
  uint8_t avail[HEALTH_ACTS];
  uint8_t n = 0;
  for (uint8_t i = 0; i < HEALTH_ACTS; i++)
    if (healthCount[i] < HEALTH_QUOTA[i]) avail[n++] = i;
  if (n == 0) return -1;
  return (int8_t)avail[random(n)];
}

void healthRollDay()  // auto-reset on day change (called at trigger eval)
{
  if (healthDayKey != healthDayKeyNow()) healthResetCounters();
}

// serial 0x05: force a health reminder. forced 0-4 = fixed activity, pure display test (no quota counting);
// forced -1 = random pick, counts toward quota as usual. done restores the page.
void healthForceDemo(int8_t forced) {
  healthRollDay();
  if (bandPhase == BAND_HEALTH || bandPhase == BAND_FADEOUT) {
    Serial.println("[BAND] busy, try later.");
    return;
  }
  if (forced >= 0 && forced < HEALTH_ACTS) {
    healthActivity = forced;  // fixed activity for icon/content testing
  } else {
    int8_t pick = healthPick();
    if (pick < 0) {
      Serial.println("[BAND] daily quota full, no demo.");
      return;
    }
    healthActivity = pick;
    healthCount[pick]++;
    lastHealthMs = millis();
    healthSave();
  }
  bandPhase = BAND_HEALTH;
  healthStage = 0;
  healthStageStart = millis();
  healthBlFrom = blDutyNow();
  healthBlCur = healthBlFrom;
  healthBlRestore = healthBlFrom;
  healthPending = false;
  healthShownTask = true;
  lastDateAreaDraw = 0;
  Serial.printf("[BAND] demo: %s (send done to restore)\n", healthPhrase(healthActivity).c_str());
}

// RGB565 lerp: c1 at alpha=0 to c2 at alpha=255
uint16_t lerpColor(uint16_t c1, uint16_t c2, int alpha) {
  int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  int r = r1 + ((r2 - r1) * alpha) / 255;
  int g = g1 + ((g2 - g1) * alpha) / 255;
  int b = b1 + ((b2 - b1) * alpha) / 255;
  if (r < 0) r = 0;
  if (r > 31) r = 31;
  if (g < 0) g = 0;
  if (g > 63) g = 63;
  if (b < 0) b = 0;
  if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// compute the effective breath style (with running crossfade) into global bEff
void effStyle(uint32_t nowMs) {
  uint32_t dt = nowMs - bXfadeMs;
  if (dt >= PHASE_XFADE_MS) {
    bEff = bCur;
    return;
  }
  float m = dt / (float)PHASE_XFADE_MS;
  bEff.periodMs = bPrev.periodMs + (bCur.periodMs - bPrev.periodMs) * m;
  bEff.amp = bPrev.amp + (bCur.amp - bPrev.amp) * m;
  bEff.riseFrac = bPrev.riseFrac + (bCur.riseFrac - bPrev.riseFrac) * m;
  bEff.lit = lerpColor(bPrev.lit, bCur.lit, (int)(m * 255));
}

// ---------- OTA: HTTPS pull update, triggered by {"state":"ota","ver","url","md5"} ----------
// 4MB flash, 4M2M scheme: two ~1020KB OTA slots; the download streams into the
// inactive slot (no RAM buffer), EEPROM identity and the unused 2MB FS stay
// untouched. Version budget: every release bin must stay under ~1019KB.

// numeric compare of dotted versions ("V1.3.0" style, V prefix optional)
int cmpVersion(const String& a, const String& b) {
  String parts[2][8];
  int n[2] = {0, 0};
  String in[2] = {a, b};
  for (int w = 0; w < 2; w++) {
    if (in[w].length() && (in[w][0] == 'V' || in[w][0] == 'v')) in[w] = in[w].substring(1);
    while (in[w].length() && n[w] < 8) {
      int dot = in[w].indexOf('.');
      parts[w][n[w]++] = (dot < 0) ? in[w] : in[w].substring(0, dot);
      in[w] = (dot < 0) ? "" : in[w].substring(dot + 1);
    }
  }
  for (int i = 0; i < 8; i++) {
    long va = (i < n[0]) ? parts[0][i].toInt() : 0;
    long vb = (i < n[1]) ? parts[1][i].toInt() : 0;
    if (va != vb) return (va < vb) ? -1 : 1;
  }
  return 0;
}

String lastOtaError = "";

// ---- OTA boot-loop guard (anti-brick) ----
// A true brick is impossible over OTA (the bootloader at 0x0 is never written, eboot
// falls back to the old slot on a corrupt image, MD5 gates the commit). The real risk
// is a VALID image whose firmware crashes at boot -> crash loop. RTC memory survives
// soft restarts (cleared on power loss): a successful OTA arms the guard, the new
// firmware disarms it after 60s of healthy uptime, and 3 consecutive armed boots
// without disarming trip SAFE MODE: clock + NTP + the proven OTA listener only
// (weather HTTP/JSON and other heavy subsystems skipped), so a corrected payload can
// be pushed over the air - remote self-heal instead of a USB recovery trip.
#define OTA_GUARD_MAGIC 0x4F544147UL  // "OTAG"
#define OTA_GUARD_MAX_TRIES 3
// Boot-mode download: the payload is staged to EEPROM and flashed by the NEXT boot, before
// any subsystem allocates. A long-uptime heap (measured 43% fragmented, largest block 10.8KB)
// stalls the TLS handshake into the HW WDT; a pristine boot removes that variable entirely.
#define OTA_URL_ADDR 200   // staged download url, char[256] (EEPROM sector has room past 194)
#define OTA_MD5_ADDR 460   // staged md5, char[40]
// pending = "new firmware installed, counting healthy boots" / rsvd = "download staged, flash at boot"
struct OtaGuard { uint32_t magic; uint8_t pending; uint8_t bootCount; uint16_t rsvd; };
static OtaGuard otaGuard = { 0, 0, 0, 0 };
static bool safeMode = false;

void otaGuardSave() {
  ESP.rtcUserMemoryWrite(32, (uint32_t*)&otaGuard, sizeof(otaGuard));
}

void otaGuardBoot() {
  otaGuard.magic = OTA_GUARD_MAGIC;  // start from a clean record if RTC holds garbage
  ESP.rtcUserMemoryRead(32, (uint32_t*)&otaGuard, sizeof(otaGuard));
  if (otaGuard.magic != OTA_GUARD_MAGIC) {
    otaGuard.magic = OTA_GUARD_MAGIC; otaGuard.pending = 0; otaGuard.bootCount = 0; otaGuard.rsvd = 0;
    otaGuardSave();
    return;
  }
  if (otaGuard.pending) {
    otaGuard.bootCount++;
    otaGuardSave();
    Serial.printf("[BOOT] OTA guard: armed boot %u/%u\n", (unsigned)otaGuard.bootCount, (unsigned)OTA_GUARD_MAX_TRIES);
    if (otaGuard.bootCount >= OTA_GUARD_MAX_TRIES) {
      safeMode = true;
      Serial.println("[BOOT] SAFE MODE: minimal services + OTA listener only (anti crash-loop)");
    }
  }
  if (otaGuard.rsvd && otaGuard.pending) {  // impossible pair (install completed while staged): trust the install
    otaGuard.rsvd = 0;
    otaGuardSave();
  }
}

void otaGuardDisarm() {
  if (!otaGuard.pending) return;
  otaGuard.pending = 0;
  otaGuard.bootCount = 0;
  otaGuardSave();
  Serial.println("[BOOT] OTA guard disarmed: firmware confirmed healthy");
  if (safeMode) {  // the corrected payload booted fine - leave safe mode live, no reboot needed
    safeMode = false;
    Serial.println("[BOOT] leaving safe mode, restoring full services");
    getCityWeater();
  }
}

bool otaDownload(const String& url, const String& md5) {
  bool useHttps = url.startsWith("https://");
  // SEGMENTED download: one fresh connection per 256KB. Beyond one per-pcb flash-stall
  // exposure, this bounds the TIME_WAIT pcb count: 6 x 128KB cycles crashed lwip at the
  // 6th connect (pcb pool exhaustion, tcp_output wild pointer); 3 x 256KB stays well clear.
  const size_t SEG = 262144;
  size_t written = 0;   // bytes already accepted by the Updater (advances across retries too)
  int total = -1;       // full size, learned from the first 206 Content-Range
  int lastPct = -1;
  uint32_t lastBucket = 0;
  uint32_t deadline = millis() + 240000;
  int retries = 0;

  int tlsBuf = 4096, tlsTx = 1024;  // TLS record size (MFLN ladder, set in the https block below)
  if (useHttps) {
    int hs = url.indexOf("://") + 3;
    int he = url.indexOf('/', hs);
    String otaHost = url.substring(hs, he < 0 ? (int)url.length() : he);
    // warm the DNS cache first: the OTA connect must not double as the first resolver round
    IPAddress otaIp;
    int dns = WiFi.hostByName(otaHost.c_str(), otaIp);
    if (dns != 1) { lastOtaError = "dns failed"; return false; }
    Serial.printf("[OTA] dns %s, heap %u, maxblk %u, frag %u%%\n", otaIp.toString().c_str(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize(), (unsigned)ESP.getHeapFragmentation());
    // MFLN record-size ladder: bigger TLS records = fewer round trips = faster on high-RTT links
    // (CN -> OSS us-west-1 measured ~450ms per 512B record = 1.1KB/s). OSS honors 512 (measured)
    // and rejects 4096 (measured); 2048/1024 untested - probe descending, take the biggest win.
    tlsBuf = 4096; tlsTx = 1024;  // no-MFLN fallback (never taken on OSS so far)
    {
      BearSSL::WiFiClientSecure probe;
      if (probe.probeMaxFragmentLength(otaHost, 443, 2048)) { tlsBuf = 2048; tlsTx = 2048; }
      else if (probe.probeMaxFragmentLength(otaHost, 443, 1024)) { tlsBuf = 1024; tlsTx = 1024; }
      else if (probe.probeMaxFragmentLength(otaHost, 443, 512)) { tlsBuf = 512; tlsTx = 512; }
    }
    Serial.printf("[OTA] TLS record %d, heap %u\n", tlsBuf, (unsigned)ESP.getFreeHeap());
    delay(50);  // drain: the probe pcb just closed; the next TLS connect must not race its
                // teardown in the WiFi SYS context (back-to-back connect = measured stall source)
    // heap relief for the seg-0 TLS handshake: drop the NTP UDP socket (pcb + lwip buffers).
    // Restored by the caller on failure; a successful OTA reboots straight into the new build.
    Udp.stop();
  }

  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // no modem sleep: with 512B TLS records, radio naps between yields tank throughput
  while (total < 0 || written < (size_t)total) {
    if (millis() > deadline) { lastOtaError = "timeout"; Update.end(); return false; }
    int segStart = (int)written;
    int segEnd = segStart + (int)SEG - 1;
    if (total > 0 && segEnd > total - 1) segEnd = total - 1;

    HTTPClient http;
    BearSSL::WiFiClientSecure tls;
    WiFiClient plain;
    if (useHttps) {
      tls.setBufferSizes(tlsBuf, tlsTx);
      tls.setInsecure();  // v1: md5 pins the payload; cert pinning lands before volume sales
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // GitHub Releases 302s to the CDN (enum API, core 3.x)
      if (!http.begin(tls, url)) { lastOtaError = "http begin failed"; return false; }
    } else {
      if (!http.begin(plain, url)) { lastOtaError = "http begin failed"; return false; }
    }
    const char* crKey[] = { "Content-Range" };
    http.collectHeaders(crKey, 1);
    http.addHeader("Range", "bytes=" + String(segStart) + "-" + String(segEnd));
    Serial.printf("[OTA] seg@%u, heap %u, maxblk %u\n", (unsigned)segStart, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());  // segment-start heap trend: leak detector; maxblk = TLS handshake currency
    // TLS handshakes over real-internet paths stall past the 3.2s soft WDT (kernel-level wait,
    // not the Arduino loops - those all yield). Suspend the soft WDT for the connect+GET window:
    // if the wait is loop-side, the 5s socket timeout bounds it and the segment retry takes over;
    // if kernel-side, the HW WDT is the last resort. Bounded either way, never unwatched forever.
    ESP.wdtDisable();
    int code = http.GET();
    ESP.wdtEnable(0);
    if (written == 0) Serial.printf("[OTA] GET %d, heap %u\n", code, (unsigned)ESP.getFreeHeap());

    int segTotal = 0;
    if (code == 206) {
      String cr = http.header("Content-Range");  // "bytes 0-131071/709376"
      int slash = cr.lastIndexOf('/');
      if (slash < 0) { lastOtaError = "bad Content-Range"; http.end(); return false; }
      total = cr.substring(slash + 1).toInt();
      segTotal = segEnd - segStart + 1;
    } else if (code == 200 && written == 0) {
      total = http.getSize();  // server ignored Range: fall back to one full-body pass
      segTotal = total;
    } else {
      lastOtaError = "HTTP " + String(code);
      http.end();
      // transient network/TLS blips must not kill the whole transfer: resume from the exact
      // Updater position on a fresh connection (the stall path below shares this retry budget)
      if (++retries > 4) return false;
      Serial.printf("[OTA] HTTP %d at %u, retry %d\n", code, (unsigned)written, retries);
      delay(250);  // let the closed pcb drain before reconnecting
      continue;
    }
    if (total <= 0) { lastOtaError = "no total size"; http.end(); return false; }
    if (written == 0) {
      if (!Update.begin((size_t)total)) { lastOtaError = "begin failed (no space?)"; http.end(); return false; }
      if (md5.length() >= 32) Update.setMD5(md5.substring(0, 32).c_str());
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t segGot = 0;
    uint32_t lastProgress = millis();
    while (segGot < (size_t)segTotal) {
      if (millis() > deadline) { lastOtaError = "timeout"; Update.end(); http.end(); return false; }
      int avail = stream->available();
      if (avail <= 0) {
        if (millis() - lastProgress > 20000) { lastOtaError = "stalled"; break; }  // 20s: a 64KB hop at 2048B records over 200ms RTT takes ~14s - don't misjudge it as dead
        delay(1);
        continue;
      }
      int r = stream->read(buf, (size_t)avail > sizeof(buf) ? sizeof(buf) : (size_t)avail);
      if (r <= 0) { delay(1); continue; }
      if (Update.write(buf, r) != (size_t)r) { lastOtaError = "flash write failed"; break; }
      segGot += (size_t)r;
      written += (size_t)r;
      lastProgress = millis();
      if (written >> 16 != lastBucket) {  // breadcrumb every 64KB
        lastBucket = written >> 16;
        deadline = millis() + 240000;  // MOVING deadline: progress buys time (a slow-but-alive
                                       // link must not die), only a truly dead transfer expires
        Serial.printf("[OTA] %u/%u, heap %u\n", (unsigned)written, (unsigned)total, (unsigned)ESP.getFreeHeap());
      }
      int pct = (int)((written * 100) / total);
      if (pct != lastPct && pct % 10 == 0) {  // progress bar, redrawn every 10%
        lastPct = pct;
        tft.fillRect(60, 150, 120, 8, COL_BG);
        tft.fillRect(60, 150, 120 * pct / 100, 8, TFT_GREENYELLOW);
      }
      yield();  // feed WDTs + WiFi task even while data flows continuously
    }
    http.end();  // segment done: pcb retired, the next one starts fresh
    delay(50);   // let the SYS context finish processing the closed pcb before reconnecting
    if (segGot < (size_t)segTotal) {
      // short segment: the Updater position (written) is exact, so resume on a fresh connection
      if (++retries > 4) {
        if (lastOtaError.length() == 0) lastOtaError = "short segment";
        Update.end();
        return false;
      }
      Serial.printf("[OTA] segment short at %u, retry %d, heap %u\n", (unsigned)written, retries, (unsigned)ESP.getFreeHeap());
      continue;
    }
    retries = 0;
  }
  if (written != (size_t)total) { lastOtaError = "short read"; Update.end(); return false; }
  if (!Update.end()) { lastOtaError = "end/md5 failed"; return false; }  // verifies md5 when set
  Serial.printf("[OTA] wrote %u bytes, verified\n", (unsigned)written);
  return true;
}

// Boot-mode download executor: called from setup() right after WiFi, before any feature
// subsystem allocates. Runs only when startOtaUpdate has staged a payload (guard.rsvd=1).
// The flag is cleared BEFORE the download so a mid-flash power loss boots normally instead
// of re-entering the download; a successful install arms the guard and reboots into the new
// slot. Failure falls through to a normal boot - old slot intact, error left in lastOtaError.
void otaBootDownload() {
  if (otaGuard.magic != OTA_GUARD_MAGIC || otaGuard.rsvd != 1) return;
  char urlBuf[256], md5Buf[40];
  EEPROM.get(OTA_URL_ADDR, urlBuf);
  EEPROM.get(OTA_MD5_ADDR, md5Buf);
  urlBuf[sizeof(urlBuf) - 1] = 0; md5Buf[sizeof(md5Buf) - 1] = 0;
  otaGuard.rsvd = 0;  // consume the flag up front: no re-entry on crash/power loss
  otaGuardSave();
  if (urlBuf[0] == 0 || urlBuf[0] == (char)0xFF) { Serial.println("[OTA] staged flag without url, skipped"); return; }

  Serial.printf("[OTA] boot-mode download: %s\n", urlBuf);
  tft.fillScreen(COL_BG);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_GREENYELLOW, COL_BG);
  tft.drawString("Updating firmware", 120, 96, 2);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawRect(60, 150, 120, 8, TFT_WHITE);  // otaDownload paints its bar inside this frame

  if (otaDownload(String(urlBuf), String(md5Buf))) {
    Serial.println("[OTA] boot-mode install complete, rebooting into new slot");
    otaGuard.pending = 1;  // arm the crash-loop counter for the new firmware
    otaGuard.bootCount = 0;
    otaGuardSave();
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.fillRect(60, 150, 120, 8, TFT_GREEN);
    tft.drawString("Rebooting...", 120, 176, 2);
    delay(1200);
    ESP.restart();
  }
  Serial.printf("[OTA] boot-mode download failed: %s\n", lastOtaError.c_str());
  tft.setTextColor(TFT_RED, COL_BG);
  tft.fillRect(60, 150, 120, 8, TFT_RED);
  tft.drawString("OTA failed", 120, 176, 2);
  delay(3000);  // fall through: normal boot, old slot intact, guard disarmed (rsvd=0, pending=0)
}

void startOtaUpdate(const String& ver, const String& url, const String& md5) {
  Serial.printf("[OTA] request %s (running %s)\n", ver.c_str(), Version);
  if (cmpVersion(ver, Version) <= 0) { Serial.println("[OTA] skip: not newer than running"); return; }
 // v1 integrity model: the md5 arrives over the TLS-protected MQTT channel and gates the
  // payload, so the download channel may be plain http (OSS). https stays supported for
  // GitHub Releases; TLS hardening (cert pinning) remains on the roadmap.
  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    Serial.println("[OTA] skip: url must be http(s)");
    return;
  }


  // Boot-mode download: stage the payload + md5 and reboot; the NEXT boot flashes before any
  // subsystem allocates. Rationale (measured on device): a long-uptime heap at 43% fragmenta-
  // tion with a 10.8KB largest free block stalled the seg-0 TLS handshake into the HW WDT
  // (rst cause:4). A pristine boot removes runtime heap state from the equation entirely.
  char urlBuf[256]; url.toCharArray(urlBuf, sizeof(urlBuf));
  char md5Buf[40];  md5.toCharArray(md5Buf, sizeof(md5Buf));
  EEPROM.put(OTA_URL_ADDR, urlBuf);
  EEPROM.put(OTA_MD5_ADDR, md5Buf);
  EEPROM.commit();
  otaGuard.magic = OTA_GUARD_MAGIC;
  otaGuard.pending = 0;   // nothing installed yet: do not arm the crash-loop counter
  otaGuard.bootCount = 0;
  otaGuard.rsvd = 1;      // download staged: setup() flashes it before anything else allocates
  otaGuardSave();

  // static progress screen: visible only for the staged-reboot moment
  tft.fillScreen(COL_BG);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_GREENYELLOW, COL_BG);
  tft.drawString("Updating firmware", 120, 96, 2);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawString(ver.c_str(), 120, 120, 2);
  tft.drawRect(60, 150, 120, 8, TFT_WHITE);
  Serial.println("[OTA] staged for boot-mode download, rebooting");
  WiFi.disconnect();  // clean disassociate FIRST: an abrupt restart with a live association can
  delay(200);         // strand the router-side session - the next boot's join then fails into the config portal
  ESP.restart();  // setup() performs the download with a pristine heap
}

// parse and apply one MQTT message (JSON only; plain text ignored by convention)
void applyMqttMsg(const String& raw) {
  String line = raw;
  line.trim();
  if (line.length() == 0 || !line.startsWith("{")) return;

  StaticJsonDocument<512> doc;  // stack-parsed: hot path no longer churns heap allocs (anti-fragmentation)
  if (deserializeJson(doc, raw) != DeserializationError::Ok) return;

  String st = doc["state"].as<String>();
  String model = doc["model"].as<String>();
  String stLow = st;
  stLow.toLowerCase();
  bool isDone = (stLow.indexOf("done") >= 0);
  bool isGen = (stLow.indexOf("generating") >= 0);
  bool isEstimate = (stLow.indexOf("estimate") >= 0);  // estimate-only signal: bookkeeping, never drives the state machine
  bool hasEst = doc.containsKey("estSec");
  if (hasEst) workEstSec = doc["estSec"].as<float>();

  // one concise line per incoming message (state + model [+ estimate])
  if (hasEst) Serial.printf("[BAND] rx %s %s est=%.1fs\n", st.c_str(), model.c_str(), (double)workEstSec);
  else Serial.printf("[BAND] rx %s %s\n", st.c_str(), model.c_str());

  if (stLow == "ota") {  // firmware update command: {"state":"ota","ver","url","md5"}
    startOtaUpdate(doc["ver"].as<String>(), doc["url"].as<String>(), doc["md5"].as<String>());
    return;
  }

  if (isDone) {
    // done: green finish only while the orb is on stage; during intro just cancel back to date
    healthPending = false;  // drop an unshown reminder with the task
    workEstSec = -1.0f;     // clear the stale estimate
    if (bandPhase == BAND_WORKING) {
      bandPhase = BAND_FADEOUT;
      fadeStartMs = millis();
    } else if (bandPhase == BAND_INTRO) {
      bandPhase = BAND_IDLE;
      dateInStartMs = millis();  // intro canceled: date fades in as usual
      lastDateAreaDraw = 0;
    } else if (bandPhase == BAND_HEALTH) {
      healthBlFrom = healthBlCur;     // keep dimming from the current brightness (done may arrive at any stage)
      healthBlRestore = blDutyNow();  // restore target = brightness right now (survives mid-demo changes)
      healthStage = 3;                // fade the page out, dark-restore the old page, back to date
      healthStageStart = millis();
    }
    return;
  }

  bool prevGen = bandGen;
  if (!isEstimate) bandGen = isGen;  // record target phase (works mid-caption too); estimate never changes style

  if (isEstimate) {
    // estimate is metadata only: no caption/orb (no done will follow, avoid an orphan breath),
  } else if (bandPhase == BAND_IDLE) {
    if (model.length() == 0) {
      // no model name: skip caption, start breathing right away
      bandPhase = BAND_WORKING;
      bPrev = bCur = (bandGen ? BS_GEN : BS_THINK);
      orbBegin(millis());
    } else {
      introModel = model;
      introStartMs = millis();
      bandPhase = BAND_INTRO;
    }
    dateInStartMs = 0;      // the new message interrupts any fade
    healthPending = false;  // new task: reset the reminder latch
    healthShownTask = false;
    if (!hasEst) workEstSec = -1.0f;  // clear the previous task estimate unless this message carries one
    lastDateAreaDraw = 0;
  } else if (bandPhase == BAND_WORKING && isGen != prevGen) {
    // phase switch: crossfade from the current effective style over 600ms
    effStyle(millis());
    bPrev = bEff;
    bCur = (isGen ? BS_GEN : BS_THINK);
    bXfadeMs = millis();
  }

  // health trigger eval: estimate over threshold + gap + quota (log the block reason)
  if (hasEst && !isDone && !healthPending && !healthShownTask && workEstSec >= HEALTH_EST_SEC) {
    healthRollDay();
    if (millis() - lastHealthMs < HEALTH_MIN_GAP_MS)
      Serial.printf("[BAND %8u] est %.0fs: gap < 30min since last reminder, no reminder\n",
                    (unsigned)millis(), workEstSec);
    else {
      int8_t pick = healthPick();
      if (pick < 0)
        Serial.printf("[BAND %8u] est %.0fs: daily quota full, no reminder\n", (unsigned)millis(), workEstSec);
      else {
        healthActivity = pick;
        healthPending = true;
        Serial.printf("[BAND %8u] health pending: %s\n", (unsigned)millis(), healthPhrase(pick).c_str());
      }
    }
  }
}

// dispatch one freshly arrived MQTT message
void mqttDraw() {
  if (!mqttDirty) return;
  mqttDirty = false;
  applyMqttMsg(mqttMsg);
}

// intro caption: model name centered, fade in -> hold -> fade out (color lerp on black)
// caption color follows the phase target: Thinking=cream / Generating=glacier blue, previewing the orb breath;
void renderIntro(uint32_t nowMs) {
  uint32_t t = nowMs - introStartMs;
  float a;
  if (t < INTRO_IN_MS) a = (float)t / INTRO_IN_MS;
  else if (t < INTRO_IN_MS + INTRO_HOLD_MS) a = 1.0f;
  else a = 1.0f - (float)(t - INTRO_IN_MS - INTRO_HOLD_MS) / INTRO_OUT_MS;
  if (a < 0.0f) a = 0.0f;

  clk.createSprite(240, 60);
  if (!clk.getPointer()) return;  // low heap: skip this frame, retry next
  clk.fillSprite(bgColor);
  String name = introModel;
  while (clk.textWidth(name) > 230 && name.length() > 1)
    name.remove(name.length() - 1);
  clk.setTextDatum(MC_DATUM);
  uint16_t lit = (bandGen ? COL_GEN : COL_BRIDGE);
  clk.setTextColor(lerpColor(bgColor, lit, (int)(a * 243)), bgColor);  // peak ~95%, softer than full white
  clk.setFreeFont(&FreeSans12pt7b);
  clk.drawString(name, 120, 30);
  clk.pushSprite(0, 150);
  clk.deleteSprite();
}

// Health page: FULL-SCREEN takeover (not a band). Fade = backlight PWM; no 240x240 canvas (fragmentation lesson):
void healthScreenTick(uint32_t nowMs) {
  orbCanvas(false);  // the health page never renders the orb: free it on entry (idempotent), freeing 8.6KB of heap
  uint32_t t = nowMs - healthStageStart;

  if (healthStage == 0) {  // stage 0: full-screen fade-out, backlight ramps down
    float p = (t >= 500) ? 1.0f : (float)t / 500.0f;
    healthBlCur = healthBlFrom + (int)((1023 - healthBlFrom) * p);
    blWrite(healthBlCur);
    if (t >= 500) {
      healthStage = 1;
      healthStageStart = nowMs;
      tft.fillScreen(bgColor);                                                                 // dark field: black bg + colored icon + two-line copy
      TJpgDec.drawJpg(70, 32, HEALTH_ICONS[healthActivity], HEALTH_ICON_LEN[healthActivity]);  // icon 100x100 centered (screen 70-170)
      tft.setFreeFont(&FreeSansBold12pt7b);
      // both lines drawn as centered soft-white/GOLD/soft-white segment runs, all bold (uniform weight
      // reads calmer than mixed weights); the gold token pops on the dark field at ~78% white
      tft.setTextDatum(TL_DATUM);
      uint16_t softWhite = lerpColor(bgColor, COL_TEXT_HI, 200);
      for (int ln = 0; ln < 2; ln++) {
        String pre = HEALTH_LINES[healthActivity][ln][0];
        String acc = HEALTH_LINES[healthActivity][ln][1];
        String post = HEALTH_LINES[healthActivity][ln][2];
        int y = (ln == 0 ? 164 : 192) - tft.fontHeight() / 2;  // nudged down: breathing room under the icon
        int x = 120 - (tft.textWidth(pre) + tft.textWidth(acc) + tft.textWidth(post)) / 2;
        if (pre.length()) { tft.setTextColor(softWhite, bgColor); tft.drawString(pre, x, y); x += tft.textWidth(pre); }
        if (acc.length()) { tft.setTextColor(COL_BRIDGE, bgColor); tft.drawString(acc, x, y); x += tft.textWidth(acc); }
        if (post.length()) { tft.setTextColor(softWhite, bgColor); tft.drawString(post, x, y); }
      }
      tft.setTextDatum(MC_DATUM);
    }
    return;
  }

  if (healthStage == 1) {  // stage 1: backlight ramps up on the dark field
    float p = (t >= 400) ? 1.0f : (float)t / 400.0f;
    healthBlCur = 1023 - (int)((1023 - healthBlFrom) * p);
    blWrite(healthBlCur);
    if (t >= 400) {
      healthStage = 2;
      healthStageStart = nowMs;  // static poster: no per-frame animation, stillness is the elegance
    }
    return;
  }

  if (healthStage == 2) return;  // stage 2: static hold, nothing on screen needs redraw

  if (healthStage == 3) {  // stage 3: done, full-screen fade-out
    float p = (t >= 500) ? 1.0f : (float)t / 500.0f;
    healthBlCur = healthBlFrom + (int)((1023 - healthBlFrom) * p);
    blWrite(healthBlCur);
    if (t >= 500) {
      // dark-field restore: lift takeover first (clock guard reads bandPhase), repaint clock+weather; date follows via IDLE
      bandPhase = BAND_IDLE;
      tft.fillScreen(bgColor);
      orbCanvas(false);  // free the orb canvas if a mid-task takeover held it (8.6KB back)
      digitalClockDisplay(1);
      if (weatherValid) drawWeatherUI(lastWcode, lastTemp, lastHumi);
      TJpgDec.drawJpg(40, 213, temperature, sizeof(temperature));  // bottom icons are boot-time decals,
      TJpgDec.drawJpg(130, 213, humidity, sizeof(humidity));       // repaint here after fillScreen wiped them
      dateInStartMs = nowMs;                                       // date fade-in (600ms, parallel with the backlight ramp)
      lastDateAreaDraw = 0;
      healthStage = 4;
      healthStageStart = nowMs;
      Serial.printf("[BAND %8u] HEALTH -> IDLE (restoring)\n", (unsigned)nowMs);
    }
    return;
  }

  // stage 4 (bandPhase already IDLE, dispatched from drawDateArea): backlight ramps up, old page fades back
  float p = (t >= 400) ? 1.0f : (float)t / 400.0f;
  healthBlCur = 1023 - (int)((1023 - healthBlRestore) * p);
  blWrite(healthBlCur);
  if (t >= 400) healthStage = 0;
}

// breath orb: crisp core + tight halo; on FADEOUT it turns green, holds bright, then dies out
// asymmetric breath: inhale takes riseFrac (cosine ease), exhale the rest - looser than a sine wave
void renderOrb(uint32_t nowMs) {
  effStyle(nowMs);
  BreathStyle st = bEff;

  // phase accumulation: no beat jump on tempo change; clamp dt after long blocks (weather HTTP)
  uint32_t dt = nowMs - bLastMs;
  bLastMs = nowMs;
  if (dt > 200) dt = 200;
  bPhaseAcc += dt / st.periodMs;
  if (bPhaseAcc >= 1.0f) bPhaseAcc -= (float)((unsigned long)bPhaseAcc);

  // asymmetric breath envelope
  float br;
  if (bPhaseAcc < st.riseFrac) {
    float u = bPhaseAcc / st.riseFrac;
    br = 0.5f - 0.5f * cosf(3.14159265f * u);  // inhale 0->1

  } else {
    float u = (bPhaseAcc - st.riseFrac) / (1.0f - st.riseFrac);
    br = 0.5f + 0.5f * cosf(3.14159265f * u);  // exhale 1->0
  }
  br *= st.amp;

  float fade = 1.0f;  // global fade for the finish
  if (bandPhase == BAND_FADEOUT) {
    // three-stage finish: to green (0-20%), full-bright hold (20-75%), die-out (75-100%)
    float p = (nowMs - fadeStartMs) / (float)BAND_FADE_MS;
    if (p < 0.20f) st.lit = lerpColor(st.lit, COL_DONE, (int)((p / 0.20f) * 255));
    else st.lit = COL_DONE;
    br = 1.0f;  // full brightness through the finish
    if (p > 0.75f) fade = 1.0f - (p - 0.75f) / 0.25f;
  }

  const int A = (int)(br * 255 * fade);   // br and fade are never negative, no clamping needed
  const int grow = (int)(br * 5 * fade);  // inhale swell
  const int rCore = 6 + grow / 2;         // crisp core (visual anchor)
  const int rHalo = 16 + grow;            // halo outer edge (max 21px)

  // two-component build: crisp solid core + fast-falloff gradient halo;
  // the core gives the eye a sharp focus, the halo only softens the edge;
  // about 22px overall - small and bright beats big and foggy on black
  TFT_eSprite* s = sprOrbOn ? &sprOrb : &clk;  // fallback: low heap drops to a full-width temporary canvas
  uint8_t cx = sprOrbOn ? ORB_CX : 120;
  uint8_t cy = sprOrbOn ? ORB_CY : 29;
  if (!sprOrbOn) {  // fallback canvas is 8-bit: degraded look, still works
    s->setColorDepth(8);
    s->createSprite(240, 60);
  }
  s->fillSprite(bgColor);
  for (int r = rHalo; r > rCore; --r) {                     // outer to inner: alpha ramps up only near the core
    float t = (float)(rHalo - r) / (float)(rHalo - rCore);  // 0 = outer edge, 1 = at the core
    int a = (int)(A * powf(t, 2.4f));
    if (a > 0) s->fillCircle(cx, cy, r, lerpColor(bgColor, st.lit, a));
  }
  int aC = A + (45 * A) / 255;  // core slightly over the envelope for a crisper focus
  if (aC > 255) aC = 255;
  s->fillCircle(cx, cy, rCore, lerpColor(bgColor, st.lit, aC));
  if (A > 200) {  // at peak the core center glows white-hot, like a real flame
    s->fillCircle(cx, cy, rCore / 2, lerpColor(st.lit, 0xFFFF, (A - 200) * 2));
  }
  if (sprOrbOn) {
    s->pushSprite(ORB_PUSH_X, 150);
  } else {
    s->pushSprite(0, 150);
    clk.deleteSprite();
  }
}

// date band state machine: default date; estimate -> caption -> breath orb; done -> green flash -> date.
void drawDateArea() {
  const uint32_t nowMs = millis();

  // ---- state advance (every call, not throttled) ----
  if (bandPhase == BAND_INTRO && nowMs - introStartMs >= INTRO_IN_MS + INTRO_HOLD_MS + INTRO_OUT_MS) {
    if (healthPending) {
      // long task: caption finished, full-screen health takeover
      Serial.printf("[BAND %8u] INTRO -> HEALTH: %s\n", (unsigned)nowMs, healthPhrase(healthActivity).c_str());
      bandPhase = BAND_HEALTH;
      healthStage = 0;  // start with the full-screen fade-out (backlight down)
      healthStageStart = nowMs;
      healthBlFrom = blDutyNow();
      healthBlCur = healthBlFrom;
      healthBlRestore = healthBlFrom;
      healthPending = false;
      healthShownTask = true;
      healthCount[healthActivity]++;
      lastHealthMs = nowMs;
      healthSave();
    } else {
      // caption finished: start breathing in the phase picked during the caption
      Serial.printf("[BAND %8u] INTRO -> WORKING  heap=%u blk=%u\n", (unsigned)nowMs, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
      bandPhase = BAND_WORKING;
      bPrev = bCur = (bandGen ? BS_GEN : BS_THINK);
      orbBegin(nowMs);
    }
    lastDateAreaDraw = 0;
  }
  if (bandPhase == BAND_WORKING && healthPending) {
    // long estimate mid-task: full-screen takeover
    Serial.printf("[BAND %8u] WORKING -> HEALTH: %s\n", (unsigned)nowMs, healthPhrase(healthActivity).c_str());
    bandPhase = BAND_HEALTH;
    healthStage = 0;  // start with the full-screen fade-out (backlight down)
    healthStageStart = nowMs;
    healthBlFrom = blDutyNow();
    healthBlCur = healthBlFrom;
    healthBlRestore = healthBlFrom;
    healthPending = false;
    healthShownTask = true;
    healthCount[healthActivity]++;
    lastHealthMs = nowMs;
    healthSave();
    lastDateAreaDraw = 0;
  }
  if (bandPhase == BAND_FADEOUT && nowMs - fadeStartMs >= BAND_FADE_MS) {
    Serial.printf("[BAND %8u] FADEOUT -> IDLE  heap=%u blk=%u\n", (unsigned)nowMs, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
    bandPhase = BAND_IDLE;    // finish done, back to date
    orbCanvas(false);         // free the orb canvas
    healthShownTask = false;  // a new task may re-arm the reminder
    dateInStartMs = nowMs;    // date fades in
    lastDateAreaDraw = 0;     // force the fade to start right away
  }

  if (healthStage == 4) healthScreenTick(nowMs);  // finish backlight ramp (bandPhase already IDLE, parallel with date drawing)

  if (bandPhase != BAND_IDLE) {
    // ---- animation throttle: breath/caption 33ms (30fps), finish 25ms (40fps) ----
    uint32_t minInterval = (bandPhase == BAND_FADEOUT) ? 25 : 33;
    if (nowMs - lastDateAreaDraw < minInterval) return;
    lastDateAreaDraw = nowMs;

    if (bandPhase == BAND_HEALTH) {
      healthScreenTick(nowMs);
      return;
    }
    clk.setColorDepth(8);  // the caption is flat color,
    // the orb has its own 16-bit resident canvas (see renderOrb)
    if (bandPhase == BAND_INTRO) renderIntro(nowMs);
    else renderOrb(nowMs);
    return;
  }

  // ---- IDLE: date fade-in after done (30fps repaint, bg -> ink2) ----
  if (dateInStartMs != 0 && nowMs - dateInStartMs < DATE_IN_MS) {
    if (nowMs - lastDateAreaDraw < 33) return;
    lastDateAreaDraw = nowMs;

    float a = (float)(nowMs - dateInStartMs) / DATE_IN_MS;
    uint16_t cIn = lerpColor(bgColor, COL_INK_2, (int)(a * 255));
    clk.setColorDepth(8);
    if (!clk.createSprite(240, 60)) {        // low heap: paint straight to screen, the date is essential
      tft.fillRect(0, 150, 240, 60, bgColor);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(cIn, bgColor);
      tft.setFreeFont(&FreeSans12pt7b);
      tft.drawString(week() + ", " + monthDay(), 120, 180);
      return;
    }
    clk.fillSprite(bgColor);
    clk.setTextDatum(MC_DATUM);
    clk.setTextColor(cIn, bgColor);
    clk.setFreeFont(&FreeSans12pt7b);
    clk.drawString(week() + ", " + monthDay(), 120, 30);
    clk.pushSprite(0, 150);
    clk.deleteSprite();
    return;
  }
  dateInStartMs = 0;  // fade complete (or self-healed), back to regular drawing

  // ---- IDLE: throttled fall-through to the date drawing below ----
  if (nowMs - lastDateAreaDraw < 500) return;
  lastDateAreaDraw = nowMs;

  clk.setColorDepth(8);

  // default: current date (western style Wed, Aug 14; ink2, quiet layer);
  // shares the same 240x60 canvas and center as caption/orb (local 120,30 = screen 120,180);
  // zero position jump across the three states
  String dateStr = week() + ", " + monthDay();
  static String lastDateDrawn = "";
  if (dateStr == lastDateDrawn) return;  // skip redraw when unchanged: kills per-500ms string heap churn at idle
  lastDateDrawn = dateStr;
  if (!clk.createSprite(240, 60)) {  // low heap: paint straight to screen
    tft.fillRect(0, 150, 240, 60, bgColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_INK_2, bgColor);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString(dateStr, 120, 180);
    return;
  }
  clk.fillSprite(bgColor);
  clk.setTextDatum(MC_DATUM);
  clk.setTextColor(COL_INK_2, bgColor);
  clk.setFreeFont(&FreeSans12pt7b);
  clk.drawString(dateStr, 120, 30);
  clk.pushSprite(0, 150);
  clk.deleteSprite();
}

// MQTT loop maintenance
void mqttLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!deviceProvisioned()) return;  // no identity yet: stay offline, clock still runs
  if (!mqttClient.connected()) {
    if (millis() - mqttReconnectT > 5000) {
      mqttReconnectT = millis();
      mqttConnect();
    }
  } else {
    mqttClient.loop();
  }
}
#endif

// web server functions
// settings page
void handleconfig() {
  String msg;
  int web_setro, web_lcdbl;

  if (server.hasArg("web_bl") || server.hasArg("web_set_rotation")) {
    web_setro = server.arg("web_set_rotation").toInt();
    web_lcdbl = server.arg("web_bl").toInt();
    Serial.println("");
    // the settings page never touches the city: no IP locate / weather fetch (boot and timer handle weather)
    if (web_lcdbl > 0 && web_lcdbl <= 100) {
      EEPROM.write(BL_addr, web_lcdbl);   // write brightness
      EEPROM.commit();                   // commit
      delay(5);
      LCD_BL_PWM = EEPROM.read(BL_addr);
      delay(5);
      Serial.printf("Brightness set to: ");
      blWrite(1023 - (LCD_BL_PWM * 10));
      Serial.println(LCD_BL_PWM);
      Serial.println("");
    }
    EEPROM.write(Ro_addr, web_setro);
    EEPROM.commit();  // commit
    delay(5);
    if (server.hasArg("web_units")) {  // temperature units: 0 = Celsius, 1 = Fahrenheit
      tempUnits = server.arg("web_units").toInt() ? 1 : 0;
      EEPROM.write(UN_addr, tempUnits);
      EEPROM.commit();
      delay(5);
      Serial.printf("[SYS] temperature units: %s\n", tempUnits ? "Fahrenheit" : "Celsius");
      if (weatherValid) drawWeatherUI(lastWcode, lastTemp, lastHumi);  // repaint the value row instantly
    }
    if (web_setro != LCD_Rotation) {
      LCD_Rotation = web_setro;
      tft.setRotation(LCD_Rotation);
      tft.fillScreen(COL_BG);
      LCD_reflash(1);  // repaint static frame
      UpdateWeater_en = 1;
      TJpgDec.drawJpg(40, 213, temperature, sizeof(temperature));  // temperature icon (bottom row)
      TJpgDec.drawJpg(130, 213, humidity, sizeof(humidity));       // humidity icon (bottom row, side by side)
    }
    Serial.print("LCD Rotation:");
    Serial.println(LCD_Rotation);
  }

  // web page markup
  String content = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  content += "<style>html,body{ background: #1aceff; color: #fff; font-family: 'Segoe UI', Arial, sans-serif; font-size: 14px; margin: 20px;}";
  content += "h1{ font-size: 22px; font-weight: 300; } .field{ margin: 12px 0; } label{ font-weight: 600; }";
  content += "input[type='text']{ width: 100%; padding: 8px; border: none; border-radius: 4px; }";
  content += "input[type='submit']{ background: #fff; color: #1aceff; border: none; padding: 10px 24px; border-radius: 20px; font-size: 14px; font-weight: 600; cursor: pointer; }</style>";
  content += "</head><body><h1>rubato Settings</h1>";
  content += "<form action='/' method='POST'>";
  content += "<div class='field'><label>Backlight (1-100)</label><br><input type='text' name='web_bl' value='" + String(LCD_BL_PWM) + "'></div>";
  content += "<div class='field'><label>Screen Rotation</label><br>";
  { const char* roName[4] = { "USB port down", "USB port right", "USB port up", "USB port left" };
    for (uint8_t r = 0; r < 4; r++)
      content += String("<input type='radio' name='web_set_rotation' value='") + r + "'" + (r == LCD_Rotation ? " checked" : "") + ">" + roName[r] + "<br>"; }
  content += "</div>";
  content += "<div class='field'><label>Temperature Units</label><br>";
  content += String("<input type='radio' name='web_units' value='0'") + (tempUnits == 0 ? " checked" : "") + "> Celsius (&deg;C)<br>";
  content += String("<input type='radio' name='web_units' value='1'") + (tempUnits == 1 ? " checked" : "") + "> Fahrenheit (&deg;F)</div>";
  content += "<div class='field'><input type='submit' name='Save' value='Save Settings'></div>" + msg;
  content += "</form></body></html>";
  server.send(200, "text/html", content);
}

//no need authentication
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// web server init
void Web_Sever_Init() {
  uint32_t counttime = 0;  // mDNS start timestamp
  Serial.println("mDNS responder building...");
  counttime = millis();
  while (!MDNS.begin("rubato")) {
    if (millis() - counttime > 30000) ESP.restart();  // reboot if mDNS takes longer than 30s
  }

  Serial.println("mDNS responder started");

  server.on("/", handleconfig);
  server.onNotFound(handleNotFound);

  // start the TCP server
  server.begin();
  Serial.println("HTTP server started");

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  // advertise the server over mDNS
  MDNS.addService("http", "tcp", 80);
}
// web settings pump
void Web_Sever() {
  MDNS.update();
  server.handleClient();
}
// LCD shows the URL and IP once the web server is up
void Web_sever_Win() {
  clk.setColorDepth(8);

  clk.createSprite(200, 70);  // create window
  clk.fillSprite(COL_BG);     // fill

  tft.fillRect(0, 0, 240, 108, COL_BG);  // clear the wordmark strip: the IP screen owns the top

  clk.setTextDatum(CC_DATUM);  // text datum
  clk.setTextColor(TFT_GREEN, COL_BG);
  clk.drawString("Open Browser:", 100, 15, 2);
  clk.setTextColor(TFT_WHITE, COL_BG);
  clk.drawString(WiFi.localIP().toString(), 100, 40, 2);
  clk.pushSprite(20, 40);  // window position

  clk.deleteSprite();
}

#if WM_EN
// WiFi-config LCD screen
void Web_win() {
  tft.fillRect(0, 0, 240, 108, COL_BG);  // clear the wordmark strip: the fail text owns the top
  clk.setColorDepth(8);

  clk.createSprite(200, 60);  // create window
  clk.fillSprite(COL_BG);     // fill

  clk.setTextDatum(CC_DATUM);  // text datum
  clk.setTextColor(TFT_GREEN, COL_BG);
  clk.drawString("WiFi Connect Fail!", 100, 10, 2);
  clk.drawString("SSID", 45, 40, 2);
  clk.setTextColor(TFT_WHITE, COL_BG);
  clk.drawString("RUBATO", 125, 40, 2);
  clk.pushSprite(20, 50);  // window position

  clk.deleteSprite();
}

// WiFi-config portal
void Webconfig() {
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP

  delay(3000);
  // NOTE: no wm.resetSettings() here - it erased the saved WiFi on every
  // portal entry, so one slow router moment (>7s at boot) wiped credentials.
  // wm.autoConnect tries the stored credentials first and only opens the
  // portal when they fail; 0x04 stays the one and only manual wipe.

  // add a custom input field
  int customFieldLength = 40;
  const char* set_rotation = "<br/><label for='set_rotation'>Screen Rotation</label>\
                              <input type='radio' name='set_rotation' value='0' checked> USB port down<br>\
                              <input type='radio' name='set_rotation' value='1'> USB port right<br>\
                              <input type='radio' name='set_rotation' value='2'> USB port up<br>\
                              <input type='radio' name='set_rotation' value='3'> USB port left<br>";
  WiFiManagerParameter custom_rot(set_rotation);  // custom html input
  WiFiManagerParameter custom_bl("LCDBL", "LCD BackLight(1-100)", "30", 3);
  WiFiManagerParameter p_lineBreak_notext("<p></p>");

  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_bl);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_rot);
  wm.setSaveParamsCallback(saveParamCallback);
  std::vector<const char*> menu = { "wifi", "restart" };
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");
  wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%

  wm.setConnectTimeout(10);  // cap wm's own saved-credential retry so the portal still shows fast
  bool res;
  res = wm.autoConnect("RUBATO");  // anonymous ap

  while (!res)
    ;
}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");

  // persist the values from the page
  LCD_Rotation = getParam("set_rotation").toInt();
  LCD_BL_PWM = getParam("LCDBL").toInt();

  // apply the values
  // city: force IP auto-locate after provisioning (setup never sets a city)
  EEPROM.write(CC_addr, 0);
  EEPROM.commit();
  cityCode = "";
  // rotation
  Serial.print("LCD_Rotation = ");
  Serial.println(LCD_Rotation);
  if (EEPROM.read(Ro_addr) != LCD_Rotation) {
    EEPROM.write(Ro_addr, LCD_Rotation);
    EEPROM.commit();
    delay(5);
  }
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(COL_BG);
  Web_win();
  loadNum--;
  loading(1);
  if (EEPROM.read(BL_addr) != LCD_BL_PWM) {
    EEPROM.write(BL_addr, LCD_BL_PWM);
    EEPROM.commit();
    delay(5);
  }
  // brightness
  Serial.printf("Brightness set to: ");
  blWrite(1023 - (LCD_BL_PWM * 10));
  Serial.println(LCD_BL_PWM);
}
#endif





void setup() {
  Serial.begin(115200);
  otaGuardBoot();  // anti-brick: count armed boots, trip safe mode on a crash loop
  EEPROM.begin(1024);
  healthLoad();  // load today reminder counters (auto-reset on day change)
  lastHealthMs = millis() - HEALTH_MIN_GAP_MS;  // fresh boot: no phantom cooldown, the first
                                                // qualifying estimate may fire right away
  // wm.resetSettings();    // uncomment to wipe saved WiFi on boot

  // backlight from EEPROM
  if (EEPROM.read(BL_addr) > 0 && EEPROM.read(BL_addr) < 100)
    LCD_BL_PWM = EEPROM.read(BL_addr);
  pinMode(LCD_BL_PIN, OUTPUT);
  blWrite(1023 - (LCD_BL_PWM * 10));
  // rotation from EEPROM
  if (EEPROM.read(Ro_addr) >= 0 && EEPROM.read(Ro_addr) <= 3)
    LCD_Rotation = EEPROM.read(Ro_addr);
  // temperature units from EEPROM (0xFF fresh flash reads as Celsius default)
  if (EEPROM.read(UN_addr) == 1) tempUnits = 1;
  // persisted timezone offset (last known Open-Meteo utc_offset_seconds); sane-range guard.
  // Real offsets are always multiples of 900s (15 min) - rejects never-written 0xFF (= -1) and garbage.
  EEPROM.get(4, tzOffsetSec);
  if (tzOffsetSec < -12 * 3600L || tzOffsetSec > 14 * 3600L || tzOffsetSec % 900L != 0) tzOffsetSec = 8 * 3600L;
#if MQTT_EN
  loadDeviceInfo();  // deviceId/token from EEPROM (0x06 serial provisioning)
#endif



  tft.begin();           /* TFT init */
  tft.invertDisplay(1);  // invert all colors: 1 = inverted, 0 = normal
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(COL_BG);
  tft.setTextColor(TFT_BLACK, bgColor);

  targetTime = millis() + 1000;
  readwificonfig();  // read stored wifi info
  Serial.print("Connecting to WiFi ");
  Serial.println(wificonf.stassid);
  WiFi.persistent(false);  // sketch EEPROM is the source of truth - stop the SDK from
                           // keeping its own NVS copy (diverged NVS creds = silent slow boots)
  uint32_t wifiT0 = millis();
  WiFi.begin(wificonf.stassid, wificonf.stapsw);
  drawBootTitle();  // small wordmark, top center: hidden whenever boot status text takes over

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  int barCycles = 0;
  while (WiFi.status() != WL_CONNECTED) {
    loading(30);

    if (loadNum >= 194) {
      // one full bar without a link: clear it and try again - routers can be
      // slow (DHCP, 2.4G congestion, AP reboot); surrender to the portal only
      // after 3 full bars (~20s), never wiping the stored credentials
      loadNum = 0;
      WiFi.reconnect();  // re-kick association: a stalled first attempt often links in seconds
      barCycles++;
      Serial.printf("[WIFI] still down, retry #%d t=%lu ms\r\n", barCycles, millis() - wifiT0);
      if (barCycles >= 3) {
        // with WEB config on, smartconfig is retired
        Web_win();
        Webconfig();
        break;
      }
    }
  }
  delay(10);
  while (loadNum < 194)  // let the animation finish
  {
    loading(1);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] connected '%s' t=%lu ms\r\n", WiFi.SSID().c_str(), millis() - wifiT0);
    // Serial.print("SSID:");
    // Serial.println(WiFi.SSID().c_str());
    // Serial.print("PSW:");
    // Serial.println(WiFi.psk().c_str());
    strcpy(wificonf.stassid, WiFi.SSID().c_str());  // copy ssid
    strcpy(wificonf.stapsw, WiFi.psk().c_str());    // copy password
    savewificonfig();  // persist the actual connection (portal-entered creds land here)
    // start the web server early so weather shows sooner
    Web_Sever_Init();
    Web_sever_Win();
    delay(3000);
  }

  // boot-mode OTA: flash a staged payload at the cleanest heap the device ever sees.
  // No WiFi yet (config portal path)? keep the staged flag - the next connected boot retries.
  if (WiFi.status() == WL_CONNECTED) otaBootDownload();

  // Serial.print("Local IP: ");
  // Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("Waiting for time sync...");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);



  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  // read the saved city
  String cityFromEEP = readCityNamefromEEP();

  tft.fillScreen(COL_BG);  // clear screen

  TJpgDec.drawJpg(40, 213, temperature, sizeof(temperature));  // temperature icon (bottom row)
  TJpgDec.drawJpg(130, 213, humidity, sizeof(humidity));       // humidity icon (bottom row, side by side)

  // 1. saved city: use it directly
  if (cityFromEEP.length() >= 1) {
    cityCode = cityFromEEP;
  }
  // 2. no saved city: the weather fetch IP-locates internally
  else {
    cityCode = "";
  }

  // wait for NTP so the clock never shows 00:00 (3 tries, 1.5s each)
  for (int i = 0; i < 3; i++) {
    if (now() > 100000) break;  // time already synced (>1970)
    time_t tsync = getNtpTime();
    if (tsync > 100000) {
      setTime(tsync);
      break;
    }
  }

#if MQTT_EN
  // init MQTT: per-device identity, TLS to the managed broker. No NTP gate
  // anymore - the dynamic-signature timestamp requirement died with Aliyun.
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);    // generous keepalive keeps reconnects rare
  mqttClient.setBufferSize(512);  // records carry model names and token stats
  mqttClient.setSocketTimeout(5); // fail fast when the broker is unreachable
  if (deviceProvisioned()) {
    mqttConnect();
  } else {
    Serial.print("[MQTT] unprovisioned, suggested id: ");
    Serial.println(suggestedId);
  }
#endif

  // draw the clock and all non-weather elements first
  digitalClockDisplay(1);

  if (safeMode) {  // top strip is free (weather skipped): make the recovery state visible
    tft.setTextColor(TFT_RED, COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("SAFE MODE", 120, 4, 2);
    tft.setTextDatum(TL_DATUM);
  }

  // weather last (empty cityCode IP-locates first; failure falls back to Shanghai)
  if (!safeMode) getCityWeater();
}



void loop() {
  if (otaGuard.pending && millis() > 60000) otaGuardDisarm();  // 60s crash-free = healthy
  Web_Sever();
#if MQTT_EN
  mqttLoop();
  mqttDraw();
  drawDateArea();  // high-frequency date band (40fps animating, 2fps idle, throttled inside)
#endif
  LCD_reflash(0);
  Serial_set();
}

void LCD_reflash(int en) {
  if (now() != prevDisplay || en == 1) {
    prevDisplay = now();
    digitalClockDisplay(en);
    prevTime = 0;
  }

  // every 2 seconds
  if (second() % 2 == 0 && prevTime == 0 || en == 1) {
    // reserved: periodic refresh hook
  }


  if (!safeMode) {  // safe mode: no weather HTTP/JSON (the OTA listener is the lifeline)
    if (millis() - weaterTime > (60000UL * WEATHER_REFRESH_MIN) || en == 1 || UpdateWeater_en != 0) {  // weather on a fixed interval
      if (WiFi.status() == WL_CONNECTED) {
        // Serial.println("WIFI connected");
        getCityWeater();
        if (UpdateWeater_en != 0) UpdateWeater_en = 0;
        weaterTime = millis();
        // while(!getNtpTime());
        getNtpTime();
      }
    }
  }
}

// IP locate: city name + lat/lon via ip-api.com (free, no key)
void getCityCode() {
  String URL = "http://ip-api.com/json/?fields=status,message,city,lat,lon";
  // HTTPClient object
  HTTPClient httpClient;

  // configure the request URL
  httpClient.begin(wificlient, URL);
  httpClient.setTimeout(5000);

  // set the User-Agent header
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");

  // connect and send the GET request
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);

  // on HTTP 200: parse the response body
  if (httpCode == HTTP_CODE_OK) {
    String str = httpClient.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, str);
    if (doc["status"].as<String>() == "success") {
      String cn = doc["city"].as<String>();
      cn.trim();
      if (cn.length() > 0) {
        cityCode = cn;
        saveCityNametoEEP(cityCode);
        Serial.print("City (IP): ");
        Serial.println(cityCode);
      }
      // store lat/lon for the Open-Meteo query
      wLat = doc["lat"].as<float>();
      wLon = doc["lon"].as<float>();
      Serial.print("LatLon: ");
      Serial.print(wLat, 4);
      Serial.print(", ");
      Serial.println(wLon, 4);
    } else {
      Serial.print("IP locate failed: ");
      Serial.println(doc["message"].as<String>());
    }
  } else {
    Serial.println("IP locate request error: ");
    Serial.println(httpCode);
  }

  // close the connection
  httpClient.end();
}

// wind bearing to abbreviation
String windDir(int deg) {
  if (deg < 23) return "N";
  if (deg < 68) return "NE";
  if (deg < 113) return "E";
  if (deg < 158) return "SE";
  if (deg < 203) return "S";
  if (deg < 248) return "SW";
  if (deg < 293) return "W";
  if (deg < 338) return "NW";
  return "N";
}

// Open-Meteo (WMO) weather code to the bundled icon index
int wmoIcon(int code) {
  if (code == 0) return 0;                 // clear sky, t0
  if (code == 1) return 1;                // mainly clear, t1
  if (code == 2) return 2;                // partly cloudy, t2
  if (code == 3) return 9;                // overcast, t9
  if (code == 45 || code == 48) return 18;// fog / rime fog, t18
  if (code >= 51 && code <= 57) return 3; // drizzle, t3
  if (code >= 61 && code <= 67) return 7; // freezing rain, t7
  if (code >= 71 && code <= 77) return 14;// snow, t14
  if (code >= 80 && code <= 82) return 7; // rain showers, t7
  if (code == 85 || code == 86) return 14;// snow showers, t14
  if (code >= 95 && code <= 99) return 4; // thunderstorm, t4
  return 99;
}

// fetch weather (Open-Meteo, WMO weather code)
void getCityWeater() {
  // no coordinates yet: try IP locate first
  if (wLat == 0 && wLon == 0) {
    Serial.println("No location saved, locating by IP first...");
    getCityCode();
  }
  // IP locate failed: default to Shanghai (31.23, 121.47)
  if (wLat == 0 && wLon == 0) {
    wLat = 31.23;
    wLon = 121.47;
    Serial.println("IP locate failed, using default city: Shanghai (31.23,121.47)");
  }
  Serial.println("Fetching weather (Open-Meteo)...");
  String URL = "http://api.open-meteo.com/v1/forecast?latitude=" + String(wLat, 4) + "&longitude=" + String(wLon, 4) + "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto";  // timezone=auto: local offset (DST-aware) in utc_offset_seconds; default is GMT!
  // HTTPClient object
  HTTPClient httpClient;

  httpClient.begin(wificlient, URL);
  httpClient.setTimeout(8000);
  Serial.println(URL);

  // connect and send the GET request
  int httpCode = httpClient.GET();

  // on HTTP 200: parse the response body
  if (httpCode == HTTP_CODE_OK) {

    String str = httpClient.getString();
    DynamicJsonDocument doc(1536);  // response carries current_units/timezone strings: 1024 was exhausting the pool
    DeserializationError jerr = deserializeJson(doc, str);
    if (jerr != DeserializationError::Ok) Serial.printf("[WX] json parse: %s (doc used %u/%d)\n", jerr.c_str(), (unsigned)doc.memoryUsage(), 1536);

    if (doc["current"].containsKey("weather_code")) {
      // draw the weather UI (icon, temp/humidity, city)
      weaterData(&doc);
      Serial.println("Weather data received");

    } else {
      Serial.print("Weather API error: ");
      Serial.println(str.substring(0, 80));
    }

  } else {
    Serial.print("Weather request error: HTTP ");
    Serial.println(httpCode);
    Serial.println("Check WiFi / network access to api.open-meteo.com");
  }

  // close the connection
  httpClient.end();
}


// weather/city/icon painting (called after parsing; also on health-page restore)
void drawWeatherUI(int wcode, int tempI, int humi) {
  String cname = cityCode;

  /*** paint the text ***/
  clk.setColorDepth(8);

  // temp icon + value + humidity icon + value: one row, bottom-aligned (FreeSans9pt7b)
  // temp: digits + a hand-drawn degree circle + C/F (no unit glyph in any loaded font - composed)
  // ink3: quiet, never competes
  clk.createSprite(72, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(COL_INK_3, bgColor);
  clk.setFreeFont(&FreeSans9pt7b);
  int shownTemp = tempI;
  if (tempUnits) shownTemp = (int)lroundf(tempI * 9.0f / 5.0f + 32.0f);  // C -> F conversion for display
  String tstr = String(shownTemp);
  clk.drawString(tstr, 0, 6);                        // digits (light, pale gray)
  int tw = clk.textWidth(tstr);
  clk.fillCircle(tw + 3, 9, 2, COL_INK_3);           // hand-drawn degree dot, hugs the digits
  clk.drawString(tempUnits ? "F" : "C", tw + 8, 6);  // unit letter
  clk.pushSprite(66, 213);                           // bottom-aligned with the icons (icon bottom edge 237)
  clk.deleteSprite();
  // humidity with %
  clk.createSprite(44, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(COL_INK_3, bgColor);
  clk.setFreeFont(&FreeSans9pt7b);
  clk.drawString(String(humi) + "%", 0, 6);
  clk.pushSprite(156, 213);
  clk.deleteSprite();
  // city name (left edge, FreeSansBold12pt7b; clear of the weather icon)
  clk.createSprite(150, 32);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);  // left aligned
  clk.setTextColor(COL_INK_1, bgColor);
  clk.setFreeFont(&FreeSansBold12pt7b);
  clk.drawString(cname, 10, 8);  // 10px inside the sprite
  clk.pushSprite(0, 25);         // hug the left edge
  clk.deleteSprite();

  // weather icon (WMO code to the bundled icon) - minimal mode: icon only
  // 48x48 icon, right edge at x=230 (was 60x60)
  wrat.printfweather(182, 15, wmoIcon(wcode));
}

// write the weather to screen (Open-Meteo data)
void weaterData(DynamicJsonDocument* docp) {
  JsonObject root = (*docp).as<JsonObject>();
  JsonObject cc = root["current"];

  int wcode = cc["weather_code"].as<int>();
  float temp = cc["temperature_2m"].as<float>();
  int humi = cc["relative_humidity_2m"].as<int>();

  // auto timezone: Open-Meteo returns the location's current UTC offset (DST-aware) at the root.
  // NTP yields UTC; the offset shifts the clock without a resync. Persisted at EEPROM 4-7 so a
  // reboot in a foreign zone shows local time immediately; DST transitions self-correct within
  // one weather refresh (10 min).
  long off = root["utc_offset_seconds"] | (long)tzOffsetSec;
  Serial.printf("[NTP] api offset=%ld, device offset=%ld\n", off, tzOffsetSec);  // diagnostics: proves whether the field resolved
  if (off != tzOffsetSec) {
    adjustTime(off - tzOffsetSec);  // shift the running clock, no resync needed
    tzOffsetSec = off;
    EEPROM.put(4, tzOffsetSec);
    EEPROM.commit();
    Serial.printf("[NTP] timezone offset: UTC%+ld (%ld s)\n", off / 3600, off);
  }

  // cache the latest weather: health-page takeover updates the cache only; restore repaints
  lastWcode = wcode;
  lastTemp = (int)temp;
  lastHumi = humi;
  weatherValid = true;
  if (bandPhase == BAND_HEALTH) return;  // full-screen takeover: skip drawing
  drawWeatherUI(wcode, (int)temp, humi);
}


unsigned char Hour_sign = 60;
unsigned char Minute_sign = 60;
void digitalClockDisplay(int reflash_en) {
  if (bandPhase == BAND_HEALTH) {
    prevDisplay = now();
    return;
  }                // suspend clock painting during the health-page takeover
  int timey = 84;  // clock raised 4px (was 88) to clear the message band at y=150
  // standard clock: hour(white) : colon(blinking) : minute(gray), no seconds, centered
  // 12px breathing room around the colon, 8px between digits.
  // hour tens(28) ones(72) colon(120) min tens(132) ones(176), symmetric about x=120
  if (reflash_en == 1) {
    // clear the old seconds area (x 178-230, y 110-150)
    tft.fillRect(178, 110, 52, 40, bgColor);
  }
  if (hour() != Hour_sign || reflash_en == 1)  // hour refresh
  {
    dig.printfW3660(28, timey, hour() / 10);
    dig.printfW3660(72, timey, hour() % 10);
    Hour_sign = hour();
  }
  if (minute() != Minute_sign || reflash_en == 1)  // minute refresh
  {
    dig.printfO3660(132, timey, minute() / 10);
    dig.printfO3660(176, timey, minute() % 10);
    Minute_sign = minute();
  }

  // colon (blinking): dots at y=106 / y=122, centered x=120, 8px
  // 12px spacing = hour-ones right edge 108, colon 116-124, min-tens left 132;
  // transition color between the white (left) and the orange (right);
  // cream COL_BRIDGE - soft, bridges both sides naturally
  uint16_t colonColor = COL_BRIDGE;
  if (second() % 2 == 0) {
    tft.fillCircle(120, 106, 4, colonColor);
    tft.fillCircle(120, 122, 4, colonColor);
  } else {
    tft.fillCircle(120, 106, 4, bgColor);
    tft.fillCircle(120, 122, 4, bgColor);
  }

  if (reflash_en == 1) reflash_en = 0;
    /*** date band ***/
#if MQTT_EN
  // the date band shows the message band
  drawDateArea();
#else
  clk.setColorDepth(8);

  // weekday
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(week(), 29, 16, 2);
  clk.pushSprite(102, 150);
  clk.deleteSprite();

  // month day
  clk.createSprite(95, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(monthDay(), 49, 16, 2);
  clk.pushSprite(5, 150);
  clk.deleteSprite();

#endif
  /*** date band ***/
}

// weekday
String week() {
  String wk[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  return wk[weekday() - 1];
}

// month day
String monthDay() {
  String months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  String s = months[month() - 1];
  s = s + " " + day();
  return s;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;      // NTP timestamp at bytes 40-43 of the packet
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress ntpServerIP;  // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  // try servers in rotating order: each failed attempt advances to the next one
  const char* ntpServerName = ntpServers[ntpServerIdx];
  Serial.printf("[NTP] try %s (%u/%u)\n", ntpServerName, (unsigned)(ntpServerIdx + 1), (unsigned)NTP_SERVER_COUNT);
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.printf("[NTP] synced via %s\n", ntpServerName);
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      //Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
      time_t t = (time_t)(secsSince1900 - 2208988800UL + tzOffsetSec);
      timeval tv; tv.tv_sec = t; tv.tv_usec = 0;
      settimeofday(&tv, nullptr);  // SDK clock: BearSSL cert-date checks read time(nullptr), which TimeLib's setTime never touches
      return t;
    }
  }
  Serial.println("[NTP] no response, rotating to next server");
  ntpServerIdx = (ntpServerIdx + 1) % NTP_SERVER_COUNT;
  return 0;  // 0 when the time is unavailable
}

// send the request to the NTP server
void sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
