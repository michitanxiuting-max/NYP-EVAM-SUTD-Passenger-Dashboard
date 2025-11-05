#include <lvgl.h>
#include <ui.h>
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include "lvgl_v8_port.h"
#include "waveshare_twai_port.h"
#include "can_data_parser.h"
// Weather
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Media Player - Radio Spotify
#include "esp_http_client.h"
#include "cJSON.h"

#define TAG "MEDIA_PLAYER"
#define ESP_LOGI(tag, format, ...) Serial.printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) Serial.printf("[%s] ERROR: " format "\n", tag, ##__VA_ARGS__)

// MEDIA PLAYER DATA STRUCTURES
bool radio_playing = false;
bool spotify_playing = false;
int radio_volume = 50;
int spotify_volume = 50;

// Sample track names
const char* radio_tracks[] = {
  "FM 95.8 - Classic Hits",
  "FM 101.2 - Pop Music",
  "FM 88.5 - Jazz Lounge",
  "FM 103.7 - Rock Station"
};

const char* spotify_tracks[] = {
  "Blinding Lights - The Weeknd",
  "Shape of You - Ed Sheeran",
  "Levitating - Dua Lipa",
  "Starboy - The Weeknd"
};

int current_radio_track = 0;
int current_spotify_track = 0;

// ===== CONFIG =====
const char* ssid = "Michi's iPhone (2)";
const char* password = "itsMichi15";
const char* weather_api_key = "9db52acc79376ec336f7e7b4779c9e1c";

// ===== TIMING =====
unsigned long lastUIUpdate = 0;
unsigned long lastWeatherSim = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastMapUpdate = 0;

// Data timeout tracking (ms)
#define DATA_TIMEOUT_MS 2000
unsigned long lastCANDataTime = 0;
bool dataConnected = false;
static bool need_ui_update = false;

// ===== VEHICLE DATA =====
float SOC = 0.0;
float battery_voltage = 0.0;
int highest_cell_temp = 0;
float battery_current = 0.0;
float speed_kmh = 0.0;
float wheel_fl_rpm = 0.0;
float wheel_fl_km = 0.0;
float wheel_fr_rpm = 0.0;
float wheel_fr_km = 0.0;
float wheel_bl_rpm = 0.0;
float wheel_bl_km = 0.0;
float wheel_br_rpm = 0.0;
float wheel_br_km = 0.0;
uint8_t ecu_byte0 = 255;
uint8_t ecu_byte1 = 0;
String ecu_status = "OFFLINE";

// ===== WEATHER DATA =====
String weather_condition = "Loading...";
float weather_temp = 0.0;
bool weather_heavy_rain = false;
bool weather_heavy_snow = false;
bool weather_black_ice = false;

// ===== STATE TRACKING =====
static bool driver_installed = false;
static bool speed_alert_active = false;
static bool battery_alert_active = false;
static bool can_error_alert_active = false;
static bool weather_alert_active = false;

// ===== BUFFERS =====
char buffer_overall_speed[32];
char buffer_batt_percent[16];
char buffer_batt_volts[16];
char buffer_batt_temp[16];
char buffer_fl_speed[16];
char buffer_fr_speed[16];
char buffer_bl_speed[16];
char buffer_br_speed[16];
char buffer_ecu[16];
char buffer_temp_percent[16];

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ===== WIFI SETUP =====
void wifiSetup() 
{
  printf("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) 
  {
    delay(500);
    printf(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) 
  {
    printf("\nWiFi connected: %s\n", WiFi.SSID().c_str());
  } 
  else 
  {
    printf("\nWiFi failed\n");
  }
}

// Media Player callback 
// for Radio Play/Pause button
void radio_play_pause_event(lv_event_t * e) {
  radio_playing = !radio_playing;
  
  if (radio_playing) {
    lv_label_set_text(ui_button_label2, LV_SYMBOL_PAUSE);
    Serial.println("Spotify: Playing");
  } else {
    lv_label_set_text(ui_button_label2, LV_SYMBOL_PLAY);
    Serial.println("Spotify: Paused");
  }
}
// for Spotify Play/Pause button
void spotify_play_pause_event(lv_event_t * e) {
  spotify_playing = !spotify_playing;
  
  if (spotify_playing) {
    lv_label_set_text(ui_button_label3, LV_SYMBOL_PAUSE);
    Serial.println("Radio: Playing");
  } else {
    lv_label_set_text(ui_button_label3, LV_SYMBOL_PLAY);
    Serial.println("Radio: Paused");
  }
}

// Event callback for Radio volume slider
void radio_volume_event(lv_event_t * e) {
  radio_volume = lv_slider_get_value(ui_Slider1);
  // Update volume display if you have one
  Serial.print("Radio Volume: ");
  Serial.println(radio_volume);
}

// Event callback for Spotify volume slider
void spotify_volume_event(lv_event_t * e) {
  spotify_volume = lv_slider_get_value(ui_Slider3);
  // Update volume display if you have one
  Serial.print("Spotify Volume: ");
  Serial.println(spotify_volume);
}

// Function to change radio station (can be called by next/prev buttons)
void change_radio_station() {
  current_radio_track = (current_radio_track + 1) % 4;
  lv_label_set_text(ui_radio_label, radio_tracks[current_radio_track]);
}

// Function to change Spotify track (can be called by next/prev buttons)
void change_spotify_track() {
  current_spotify_track = (current_spotify_track + 1) % 4;
  lv_label_set_text(ui_spotify_label, spotify_tracks[current_spotify_track]);
}

void setup_entertainment_page() {
  // Initialize Radio Section
  // Set initial track name
  lv_label_set_text(ui_radio_label, radio_tracks[0]);
  
  // Set initial play button icon
  lv_label_set_text(ui_button_label2, LV_SYMBOL_PLAY);
  
  // Configure volume slider
  lv_slider_set_range(ui_Slider1, 0, 100);
  lv_slider_set_value(ui_Slider1, radio_volume, LV_ANIM_OFF);
  
  // Add event listeners
  lv_obj_add_event_cb(ui_Slider1, radio_volume_event, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_t * radio_button = lv_obj_get_parent(ui_button_label2); // Get button object
  lv_obj_add_event_cb(radio_button, radio_play_pause_event, LV_EVENT_CLICKED, NULL);
  
  // Set placeholder background color for radio picture (Orange/Red)
  lv_obj_set_style_bg_color(ui_RadioPicture, lv_color_hex(0xFF5733), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_RadioPicture, LV_OPA_COVER, LV_PART_MAIN);
  
  
  // Initialize Spotify Section
  // Set initial track name
  lv_label_set_text(ui_spotify_label, spotify_tracks[0]);
  
  // Set initial play button icon
  lv_label_set_text(ui_button_label3, LV_SYMBOL_PLAY);
  
  // Configure volume slider
  lv_slider_set_range(ui_Slider3, 0, 100);
  lv_slider_set_value(ui_Slider3, spotify_volume, LV_ANIM_OFF);
  
  // Add event listeners
  lv_obj_add_event_cb(ui_Slider3, spotify_volume_event, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_t * spotify_button = lv_obj_get_parent(ui_button_label3); // Get button object
  lv_obj_add_event_cb(spotify_button, spotify_play_pause_event, LV_EVENT_CLICKED, NULL);
  
  // Set placeholder background color for spotify picture (Spotify Green)
  lv_obj_set_style_bg_color(ui_SpotifyPicture, lv_color_hex(0x1DB954), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_SpotifyPicture, LV_OPA_COVER, LV_PART_MAIN);

  Serial.println("Entertainment page initialized!");
}

// Optional: Update function to simulate playback progress
void update_entertainment_page() {
  static uint32_t last_radio_update = 0;
  static uint32_t last_spotify_update = 0;
  uint32_t current_time = millis();
  
  // Radio: Change station every 5 seconds when playing
  if (radio_playing && (current_time - last_radio_update > 1000)) {
    last_radio_update = current_time;
    change_radio_station();
  }
  
  // Spotify: Change track every 4 seconds when playing
  if (spotify_playing && (current_time - last_spotify_update > 1000)) {
    last_spotify_update = current_time;
    change_spotify_track();
  }
}

// ===== WEATHER FUNCTIONS =====
void updateWeatherLabel_NoLock(String condition) 
{
  if (condition == "Clear" || condition == "Sunny") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "SUNNY");
    lv_label_set_text(ui_WEATHER_TBD, "SUNNY");
  }
  else if (condition == "Clouds" || condition == "Haze") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "CLOUDY");
    lv_label_set_text(ui_WEATHER_TBD, "CLOUDY");
  }
  else if (condition == "Rain" || condition == "Drizzle") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "RAINY");
    lv_label_set_text(ui_WEATHER_TBD, "RAINY");
  }
  else if (condition == "Thunderstorm") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "STORMY");
    lv_label_set_text(ui_WEATHER_TBD, "STORMY");
  }
  else if (condition == "Heavy Snow") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "SNOWFALL");
    lv_label_set_text(ui_WEATHER_TBD, "SNOWFALL");
  }
  else if (condition == "Black Ice") 
  {
    lv_label_set_text(ui_WEATHER_LABEL, "HEAVY SNOWFALL");
    lv_label_set_text(ui_WEATHER_TBD, "HEAVY SNOWFALL");
  }
  else 
  {
    lv_label_set_text(ui_WEATHER_LABEL, condition.c_str());
    lv_label_set_text(ui_WEATHER_TBD, condition.c_str());
  }
}

void update_temp_label_NoLock(float temp) 
{
  sprintf(buffer_temp_percent, "%.0f°C", temp);
  lv_label_set_text(ui_TEMP_LABEL, buffer_temp_percent);
}

void updateWeatherOverlay_NoLock()
{
  lv_color_t overlay_color;
  uint8_t overlay_opacity = 0;
  
  if (weather_condition == "Clear" || weather_condition == "Sunny") 
  {
    overlay_color = lv_color_hex(0xFFFF00);
    overlay_opacity = 80;
  }
  else if (weather_condition == "Clouds" || weather_condition == "Haze") 
  {
    overlay_color = lv_color_hex(0xCCCCCC);
    overlay_opacity = 100;
  }
  else if (weather_condition == "Rain" || weather_condition == "Drizzle") 
  {
    overlay_color = lv_color_hex(0x555555);
    overlay_opacity = 150;
  }
  else if (weather_condition == "Thunderstorm") 
  {
    overlay_color = lv_color_hex(0x333333);
    overlay_opacity = 200;
  }
  else if (weather_condition == "Heavy Snow") 
  {
    overlay_color = lv_color_hex(0xE0E0E0);
    overlay_opacity = 120;
  }
  else if (weather_condition == "Black Ice") 
  {
    overlay_color = lv_color_hex(0x1A1A2E);
    overlay_opacity = 180;
  }
  else
  {
    overlay_color = lv_color_hex(0xFFFFFF);
    overlay_opacity = 0;
  }
  
  lv_obj_set_style_bg_color(ui_weatherslider, overlay_color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_weatherslider, overlay_opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void testWeatherSimulation_DataOnly()
{
  if (millis() - lastWeatherSim < 10000) return;
  
  static int weather_cycle = 0;
  
  switch(weather_cycle) 
  {
    case 0:
      weather_condition = "Clear";
      weather_temp = 32.0;
      weather_heavy_rain = false;
      weather_heavy_snow = false;
      weather_black_ice = false;
      Serial.println("SIM: Sunny - 32°C");
      break;
    case 1:
      weather_condition = "Clouds";
      weather_temp = 28.0;
      weather_heavy_rain = false;
      weather_heavy_snow = false;
      weather_black_ice = false;
      Serial.println("SIM: Cloudy - 28°C");
      break;
    case 2:
      weather_condition = "Rain";
      weather_temp = 24.0;
      weather_heavy_rain = true;
      weather_heavy_snow = false;
      weather_black_ice = false;
      Serial.println("SIM: Rainy - 24°C");
      break;
    case 3:
      weather_condition = "Thunderstorm";
      weather_temp = 22.0;
      weather_heavy_rain = true;
      weather_heavy_snow = false;
      weather_black_ice = false;
      Serial.println("SIM: Thunderstorm - 22°C");
      break;
    case 4:
      weather_condition = "Heavy Snow";
      weather_temp = -5.0;
      weather_heavy_rain = false;
      weather_heavy_snow = true;
      weather_black_ice = false;
      Serial.println("SIM: Heavy Snow - -5°C");
      break;
    case 5:
      weather_condition = "Black Ice";
      weather_temp = -10.0;
      weather_heavy_rain = false;
      weather_heavy_snow = false;
      weather_black_ice = true;
      Serial.println("SIM: Black Ice - -10°C");
      break;
  }
  
  weather_cycle = (weather_cycle + 1) % 6;
  lastWeatherSim = millis();
}

void update_weather_DataOnly()
{
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastWeatherSim < 10000) return;
  
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=1.3521&lon=103.8198&units=metric&appid=9db52acc79376ec336f7e7b4779c9e1c";
  
  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) 
  {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) 
    {
      weather_condition = doc["weather"][0]["main"].as<String>();
      weather_temp = doc["main"]["temp"].as<float>();
      
      if (weather_condition == "Rain" || weather_condition == "Thunderstorm")
      {
        weather_heavy_rain = true;
        weather_alert_active = true;
      }
      else
      {
        weather_heavy_rain = false;
        weather_alert_active = false;
      }
      
      Serial.printf("Weather Updated: %s, Temp: %.1f°C\n", weather_condition.c_str(), weather_temp);
    }
  }
  
  http.end();
  lastWeatherSim = millis();
}

// ===== CAN DATA FUNCTIONS =====
void processCANData() 
{
  if (!driver_installed) return;

  waveshare_twai_receive();
  
  bool receivedAnyData = false;

  if (vehicleData.ecu_valid)
  {
    ecu_byte0 = vehicleData.ecu_byte0;
    ecu_byte1 = vehicleData.ecu_byte1;
    if (ecu_byte0 == 255) ecu_status = "OFFLINE";
    else if (ecu_byte0 == 1) ecu_status = "OK";
    else if (ecu_byte0 == 0) ecu_status = "ERROR";
    else ecu_status = "UNKNOWN";
    receivedAnyData = true;
  }

  if (vehicleData.data_0x24_valid)
  {
    SOC = vehicleData.SOC;
    battery_voltage = vehicleData.battery_voltage;
    highest_cell_temp = vehicleData.highest_cell_temp;
    battery_current = vehicleData.battery_current;
    receivedAnyData = true;
  }

  if (vehicleData.data_0x34_valid)
  {
    wheel_fl_rpm = vehicleData.wheel_fl_rpm;
    wheel_fl_km = vehicleData.wheel_fl_km;
    receivedAnyData = true;
  }

  if (vehicleData.data_0x35_valid)
  {
    wheel_fr_rpm = vehicleData.wheel_fr_rpm;
    wheel_fr_km = vehicleData.wheel_fr_km;
    receivedAnyData = true;
  }

  if (vehicleData.data_0x36_valid)
  {
    wheel_bl_rpm = vehicleData.wheel_bl_rpm;
    wheel_bl_km = vehicleData.wheel_bl_km;
    receivedAnyData = true;
  }

  if (vehicleData.data_0x37_valid)
  {
    wheel_br_rpm = vehicleData.wheel_br_rpm;
    wheel_br_km = vehicleData.wheel_br_km;
    receivedAnyData = true;
  }

  if (vehicleData.data_0x38_valid)
  {
    speed_kmh = vehicleData.speed_kmh;
    receivedAnyData = true;
  }

  if (receivedAnyData) 
  {
    lastCANDataTime = millis();
    if (!dataConnected) 
    {
      dataConnected = true;
      need_ui_update = true;
      Serial.println(">>> DATA CONNECTED <<<");
    }
  } 
  else 
  {
    if (dataConnected && (millis() - lastCANDataTime > DATA_TIMEOUT_MS)) 
    {
      dataConnected = false;
      need_ui_update = true;
      Serial.println(">>> DATA OFFLINE - TIMEOUT <<<");
    }
  }
}

void resetUIToOffline_NoLock()
{
  SOC = 0.0;
  battery_voltage = 0.0;
  highest_cell_temp = 0;
  battery_current = 0.0;
  speed_kmh = 0.0;
  wheel_fl_rpm = 0.0;
  wheel_fl_km = 0.0;
  wheel_fr_rpm = 0.0;
  wheel_fr_km = 0.0;
  wheel_bl_rpm = 0.0;
  wheel_bl_km = 0.0;
  wheel_br_rpm = 0.0;
  wheel_br_km = 0.0;
  ecu_status = "OFFLINE";

  battery_alert_active = false;
  speed_alert_active = false;
  can_error_alert_active = true;
  weather_alert_active = false;

  lv_label_set_text(ui_CAR_TBD, "OFFLINE");
  lv_label_set_text(ui_CAN_TBD, "OFFLINE");
  
  lv_arc_set_value(ui_can_stats_arc, 0);
  lv_arc_set_value(ui_car_stats_arc, 0);
  lv_arc_set_value(ui_batt_arc, 0);
  lv_arc_set_value(ui_fl_arc, 0);
  lv_arc_set_value(ui_fr_arc, 0);
  lv_arc_set_value(ui_bl_arc, 0);
  lv_arc_set_value(ui_br_arc, 0);

  lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);

  lv_label_set_text(ui_batt_percent, "0%");
  lv_label_set_text(ui_batt_volts, "0.0V");
  lv_label_set_text(ui_batt_temp, "0°C");
  lv_label_set_text(ui_OverallSpeed, "0 km/h");
  lv_label_set_text(ui_fl_speed, "0");
  lv_label_set_text(ui_fr_speed, "0");
  lv_label_set_text(ui_bl_speed, "0");
  lv_label_set_text(ui_br_speed, "0");

  lv_slider_set_value(ui_temp_slider, 0, LV_ANIM_OFF);

  lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_battery_related, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_overspeed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_slow_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_batt_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_resolved_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void updateUI_NoLock() {
  if (ecu_byte0 == 255) {
    sprintf(buffer_ecu, "OFFLINE");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (ecu_byte0 == 1) {
    sprintf(buffer_ecu, "OK");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    can_error_alert_active = false;
    lv_obj_set_style_text_opa(ui_canbus_error, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (ecu_byte0 == 0) {
    sprintf(buffer_ecu, "ERROR");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    sprintf(buffer_ecu, "UNKNOWN");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  lv_label_set_text(ui_CAR_TBD, buffer_ecu);
  lv_label_set_text(ui_CAN_TBD, buffer_ecu);

  lv_arc_set_value(ui_batt_arc, (int16_t)SOC);
  sprintf(buffer_batt_percent, "%.0f%%", SOC);
  sprintf(buffer_batt_volts, "%.1fV", battery_voltage);
  sprintf(buffer_batt_temp, "%d°C", highest_cell_temp);
  lv_label_set_text(ui_batt_percent, buffer_batt_percent);
  lv_label_set_text(ui_batt_volts, buffer_batt_volts);
  lv_label_set_text(ui_batt_temp, buffer_batt_temp);

  float temp_value = (float)highest_cell_temp;
  if (temp_value < 0.0f) temp_value = 0.0f;
  if (temp_value > 100.0f) temp_value = 100.0f;
  lv_slider_set_value(ui_temp_slider, (int16_t)temp_value, LV_ANIM_ON);

  bool high_temp = (temp_value > 60.0f);
  bool overheat = (temp_value > 70.0f);
  bool low_soc = (SOC < 30.0f);
  bool high_voltage = (battery_voltage > 100.1f);
  
  if (high_temp || low_soc || high_voltage) {
    battery_alert_active = true;
    lv_obj_set_style_text_opa(ui_battery_related, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_batt_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (overheat) {
      lv_obj_set_style_text_opa(ui_overheat, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_slow_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
      lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (low_soc) {
      lv_obj_set_style_text_opa(ui_low_batt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_charge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
      lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (high_voltage) {
      lv_obj_set_style_text_opa(ui_high_v, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_emergency_stop, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
      lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
  } else {
    battery_alert_active = false;
    lv_obj_set_style_text_opa(ui_battery_related, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_batt_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  float fl_percent = (wheel_fl_rpm / 1966.0f) * 100.0f;
  if (fl_percent > 100.0f) fl_percent = 100.0f;
  lv_arc_set_value(ui_fl_arc, (int16_t)fl_percent);
  sprintf(buffer_fl_speed, "%.0f", wheel_fl_km);
  lv_label_set_text(ui_fl_speed, buffer_fl_speed);

  float fr_percent = (wheel_fr_rpm / 1966.0f) * 100.0f;
  if (fr_percent > 100.0f) fr_percent = 100.0f;
  lv_arc_set_value(ui_fr_arc, (int16_t)fr_percent);
  sprintf(buffer_fr_speed, "%.0f", wheel_fr_km);
  lv_label_set_text(ui_fr_speed, buffer_fr_speed);

  float bl_percent = (wheel_bl_rpm / 1966.0f) * 100.0f;
  if (bl_percent > 100.0f) bl_percent = 100.0f;
  lv_arc_set_value(ui_bl_arc, (int16_t)bl_percent);
  sprintf(buffer_bl_speed, "%.0f", wheel_bl_km);
  lv_label_set_text(ui_bl_speed, buffer_bl_speed);

  float br_percent = (wheel_br_rpm / 1966.0f) * 100.0f;
  if (br_percent > 100.0f) br_percent = 100.0f;
  lv_arc_set_value(ui_br_arc, (int16_t)br_percent);
  sprintf(buffer_br_speed, "%.0f", wheel_br_km);
  lv_label_set_text(ui_br_speed, buffer_br_speed);

  sprintf(buffer_overall_speed, "%.0f km/h", speed_kmh);
  lv_label_set_text(ui_OverallSpeed, buffer_overall_speed);

  if (speed_kmh >= 200.0f) {
    speed_alert_active = true;
    lv_obj_set_style_text_opa(ui_overspeed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_slow_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_speed_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (speed_kmh > 245.0f)
      lv_obj_set_style_text_opa(ui_max_speed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    else
      lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    speed_alert_active = false;
    lv_obj_set_style_text_opa(ui_overspeed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_slow_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  if (weather_heavy_rain) {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_heavy_rain_alert, "----------------  Heavy Rain Alert");
    lv_label_set_text(ui_return_home, "Return to Home Base");
    lv_label_set_text(ui_ALERT_LABEL1, "Return to Home Base");
  } else if (weather_heavy_snow || weather_black_ice) {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_heavy_rain_alert, "------------------  Hazard Alert");
    lv_label_set_text(ui_return_home, "Slow Down");
    lv_label_set_text(ui_ALERT_LABEL1, "Slow Down - Black Ice Detected");
  } else {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_heavy_rain_alert, "");
    lv_label_set_text(ui_return_home, "");
    lv_label_set_text(ui_ALERT_LABEL1, "");
  }

  bool anyAlertActive = battery_alert_active || speed_alert_active || can_error_alert_active || weather_alert_active;
  if (anyAlertActive) {
    lv_obj_set_style_bg_opa(ui_alert_red, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    lv_obj_set_style_bg_opa(ui_alert_red, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void setup() {
  Serial.begin(115200);
  driver_installed = waveshare_twai_init();
  delay(1000);
  printf("\n=== Passenger Dashboard Starting ===\n");
  wifiSetup();
  printf("Initializing board...\n");
  Board* board = new Board();
  board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
  auto lcd = board->getLCD();
  lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
  auto lcd_bus = lcd->getBus();
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    static_cast<BusRGB*>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
  }
#endif
#endif

  assert(board->begin());
  printf("Initializing LVGL...\n");
  lvgl_port_init(board->getLCD(), board->getTouch());
  printf("Creating UI...\n");
  lvgl_port_lock(-1);
  ui_init();

  lv_arc_set_range(ui_can_stats_arc, 0, 100);
  lv_arc_set_range(ui_car_stats_arc, 0, 100);
  lv_arc_set_range(ui_batt_arc, 0, 100);
  lv_arc_set_range(ui_fl_arc, 0, 100);
  lv_arc_set_range(ui_bl_arc, 0, 100);
  lv_arc_set_range(ui_fr_arc, 0, 100);
  lv_arc_set_range(ui_br_arc, 0, 100);
  lv_slider_set_range(ui_temp_slider, 0, 100);
  lv_slider_set_range(ui_Slider1, 0, 100);
  lv_slider_set_range(ui_Slider3, 0, 100);

  lv_arc_set_mode(ui_can_stats_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_car_stats_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_batt_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_fl_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_bl_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_fr_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_mode(ui_br_arc, LV_ARC_MODE_NORMAL);
  lv_slider_set_mode(ui_temp_slider, LV_SLIDER_MODE_NORMAL);
  lv_slider_set_mode(ui_weatherslider, LV_SLIDER_MODE_NORMAL);
  lv_slider_set_mode(ui_Slider1, LV_SLIDER_MODE_NORMAL);
  lv_slider_set_mode(ui_Slider3, LV_SLIDER_MODE_NORMAL);

  lv_obj_clear_flag(ui_can_stats_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_car_stats_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_batt_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_fl_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_bl_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_fr_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_br_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_temp_slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_weatherslider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_button_label2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_button_label3, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_set_style_bg_opa(ui_alert_red, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_alert_red2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_alert_red3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_alert_red4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_alert_red5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_alert_red6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_text_opa(ui_battery_related, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_overspeed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_slow_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_canbus_error, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_wait_for, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_batt_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_resolved_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_opa(ui_weatherslider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  
  // Setup entertainment page
  setup_entertainment_page();

  lvgl_port_unlock();
  printf("Dashboard initialized\n");
}

// ===== MAIN LOOP =====
void loop() 
{
  // 1. Process CAN data (no lock needed)
  processCANData();

  // 2. Update weather data only - no LVGL calls
  testWeatherSimulation_DataOnly();

  // Print every 100ms - SIMPLE DEBUG
  if (millis() - lastSerialPrint >= 100)
  {
    Serial.print("CONNECTED: ");
    Serial.print(dataConnected ? "YES" : "NO");
    Serial.print(" | SPEED: ");
    Serial.print(speed_kmh);
    Serial.print(" | SOC: ");
    Serial.print(SOC);
    Serial.print(" | TEMP: ");
    Serial.println(highest_cell_temp);
    lastSerialPrint = millis();
  }

  // 3. ONE BIG LOCK - all LVGL updates happen here every 30ms
  if (millis() - lastUIUpdate >= 30)
  {
    lvgl_port_lock(-1);
    
    if (need_ui_update) 
    {
      if (!dataConnected) 
      {
        resetUIToOffline_NoLock();
      }
      need_ui_update = false;
    }

    if (dataConnected) 
    {
      updateWeatherLabel_NoLock(weather_condition);
      update_temp_label_NoLock(weather_temp);
      updateWeatherOverlay_NoLock();
      update_entertainment_page();
      updateUI_NoLock();
    }
    
    lvgl_port_unlock();
    lastUIUpdate = millis();
  }
}