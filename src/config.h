#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

//------------------------------------------------------------------------------
// WiFi and MQTT Settings
//------------------------------------------------------------------------------
#define WIFI_SSID "Ghost"
#define WIFI_PASSWORD "zounette"

#define MQTT_SERVER "192.168.1.20"
#define MQTT_PORT 1883
#define MQTT_TOPIC_VALEURS "domos/piscine/valeurs"
#define MQTT_TOPIC_POMPE "domos/piscine/pompe"
#define MQTT_CLIENT_ID "M5StickFire_Piscine"

//------------------------------------------------------------------------------
// OneWire / Temperature Sensors Settings
//------------------------------------------------------------------------------
#define ONE_WIRE_BUS 26
static const DeviceAddress TempAirAddr = {0x28, 0xFF, 0x50, 0x5C,
                                          0x71, 0x17, 0x04, 0xB0};
static const DeviceAddress TempEauAddr = {0x28, 0xEA, 0xE3, 0x07,
                                          0xD6, 0x01, 0x3C, 0xFB};
static const DeviceAddress TempLocalAddr = {0x28, 0xFF, 0xF9, 0x06,
                                            0x71, 0x17, 0x04, 0xB3};

static const unsigned long TEMP_READ_INTERVAL = 10 * 1000UL;

//------------------------------------------------------------------------------
// Relay Settings (M5Stack 2-Relay Module)
//------------------------------------------------------------------------------
#define ELECTROLYSE_RELAY_INDEX 1

//------------------------------------------------------------------------------
// Display Settings
//------------------------------------------------------------------------------
#define BRIGHTNESS_MAX 200
#define BRIGHTNESS_MINI 10
#define TIME_FADE (60 * 1000UL)

//------------------------------------------------------------------------------
// NTP Settings
//------------------------------------------------------------------------------
static const char *NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 3600;
static const int DAYLIGHT_OFFSET_SEC = 3600;
static const unsigned long NTP_UPDATE_INTERVAL = 1 * 60 * 1000UL;

#endif // CONFIG_H
