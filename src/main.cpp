#include <ArduinoJson.h>
#include <algorithm>
#include <coredecls.h>                  // settimeofday_cb()
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>
#include <time.h>
#include "LittleFS.h"

#include "Arduino.h"
#include "CronAlarms.h"
#include "wifi_secrets.h"               // wifi_ssid, wifi_password
#include "activation_schedule.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

#define PUMP_PIN_1 16

#define TZ_HR -4
#define TZ_MIN ((TZ_HR)*60)
#define TZ_SEC ((TZ_HR)*3600)

#define DST_MIN 60
#define DST_SEC ((DST_MIN)*60)

timeval cbtime;         // time set in callback
bool time_set = false;

void TimeIsSet(void) {
  gettimeofday(&cbtime, NULL);
  time_set = true;
  Serial.println("------------------ settimeofday() was called ------------------");
  Serial.print("Time was set to: ");
  Serial.println(cbtime.tv_sec);
}

CronClass cron;

// Sets the device hostname to the contents of the file '/hostname'
char hostname[64] = {0};
bool GetHostname() {
  char hostname_path[] = "/hostname";
  if (!LittleFS.exists(hostname_path)) {
      Serial.println("/hostname does not exist"); 
      return false;
  }
  File f = LittleFS.open(hostname_path, "r");
  if (!f) {
      Serial.println("Could not open /hostname.");
      return false;
  }

  int bytes_read = f.readBytes(hostname, 63);
  Serial.print("Read hostname: ");
  Serial.println(hostname);
  WiFi.hostname(hostname);
  return true;
}

StaticJsonDocument<1024> ReadPinConfigs() {
    char pin_config_path[] = "/pinschedule.json";
    StaticJsonDocument<1024> pin_configs;
    if (!LittleFS.exists(pin_config_path)) {
        Serial.println("/pinschedule.json does not exist");
        return pin_configs;
    }
  File f = LittleFS.open(pin_config_path, "r");
  if (!f) {
      Serial.println("Could not open /pinschedule.json.");
      return pin_configs;
  }

  deserializeJson(pin_configs, f);
  Serial.print("Read json: ");
  serializeJson(pin_configs, Serial);
  return pin_configs;
}


void setup() {
  Serial.begin(115200);
  // Wait 10s so there's time to connect to serial port
  delay(10000);
  // Start the filesystem
  Serial.println("Starting filesystem.");
  LittleFS.begin();
  FSInfo fs_info;
  if (!LittleFS.info(fs_info)) {
      Serial.println("Failed to get filesystem info, something is wrong.");
      Serial.println("Restarting in 100s");
      delay(100000);
      ESP.restart();
  }
  Serial.print("FS size: ");
  Serial.println(fs_info.totalBytes);
  Serial.print("FS used: ");
  Serial.println(fs_info.usedBytes);

  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  // Initialize pump pins as outputs and make sure they're low
  pinMode(PUMP_PIN_1, OUTPUT);
  digitalWrite(PUMP_PIN_1, LOW);

  // Connect to wifi
  // Constants wifi_ssid and wifi_password are set in wifi_secrets.h
  if (wifi_ssid == nullptr || strlen(wifi_ssid) == 0 ||
          wifi_password == nullptr || strlen(wifi_password) == 0) {
      Serial.println("SSID and password are not set! Restarting in 100s.");
      delay(100000);
      ESP.restart();
  }
  if (!GetHostname()) {
      Serial.println("Failed to read hostname.");
  }
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Waiting for wifi");
  while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print(".");
  }
  Serial.println("\nWifi connected");

  // Configure NTP and wait for time to be set
  settimeofday_cb(TimeIsSet);
  configTime(TZ_SEC, DST_SEC, "north-america.pool.ntp.org");
  Serial.print("Waiting for NTP");
  while (!time_set) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print(".");
  }
  Serial.println("\nSystem time set.");

  // Set up watering schedule
  StaticJsonDocument<1024> schedule = ReadPinConfigs();
  PinScheduler.ParseConfig(schedule);
  
//   PinSchedule plant1 = {.pin = PUMP_PIN_1, .total_on_sec = 10, .on_sec = 4,
//       .off_sec = 6, .cron_str = "0 * * * * *"};
//   PinScheduler.Schedule(plant1);
}


void Blink(int times) {
    for (int i=0; i<times; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
    }
}

void PrintSystemTime() {
    struct timeval system_time;
    gettimeofday(&system_time, nullptr);
    Serial.print("System time: ");
    Serial.print(system_time.tv_sec);
    Serial.print("\n");
}


void loop() {
    PrintSystemTime();
    Blink(/*times=*/2);

    Cron.delay(1000);
}
