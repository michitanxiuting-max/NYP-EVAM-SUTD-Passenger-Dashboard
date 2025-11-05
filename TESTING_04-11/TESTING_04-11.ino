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
// Media Player - Radio & Spotify
#include "esp_http_client.h"
#include "cJSON.h"

#define TAG "MEDIA_PLAYER"
#define ESP_LOGI(tag, format, ...) Serial.printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) Serial.printf("[%s] ERROR: " format "\n", tag, ##__VA_ARGS__)

// ============================================
// MEDIA PLAYER DATA STRUCTURES
// ============================================

typedef struct 
{
    char name[100];
    char url[256];
    char icon[50];
} Station;

typedef struct 
{
    char name[100];
    char artist[100];
    char url[256];
    char icon[50];
} Track; 

typedef struct 
{
    bool is_playing;
    int current_index;
    int volume;
    char current_source[20]; // "radio" or "spotify"
} PlayerState;

// Global media player state
PlayerState player_state = 
{
    .is_playing = false,
    .current_index = 0,
    .volume = 70,
    .current_source = "radio"
};

// Station and track arrays
Station radio_stations[10];
int radio_count = 0;
Track spotify_tracks[10];
int spotify_count = 0;

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

// // ============================================
// // MEDIA PLAYER EVENT HANDLERS - RADIO
// // ============================================

// static void play_radio_handler(lv_event_t* e) 
// {
//     player_state.is_playing = true;
//     strcpy(player_state.current_source, "radio");
    
//     if (ui_pause) 
//     {
//         lv_obj_set_style_opa(ui_pause, LV_OPA_100, 0);
//     }
//     if (ui_play) 
//     {
//         lv_obj_set_style_opa(ui_play, LV_OPA_0, 0);
//     }
    
//     ESP_LOGI(TAG, "Playing Radio: %s", radio_stations[player_state.current_index].name);
// }

// static void pause_radio_handler(lv_event_t* e) 
// {
//     player_state.is_playing = false;
    
//     if (ui_play) 
//     {
//         lv_obj_set_style_opa(ui_play, LV_OPA_100, 0);
//     }
//     if (ui_pause) 
//     {
//         lv_obj_set_style_opa(ui_pause, LV_OPA_0, 0);
//     }
    
//     ESP_LOGI(TAG, "Radio Paused");
// }

// static void slider_radio_handler(lv_event_t* e) 
// {
//     player_state.volume = lv_slider_get_value(ui_Slider1);
//     ESP_LOGI(TAG, "Radio Volume: %d%%", player_state.volume);
// }

// // ============================================
// // MEDIA PLAYER EVENT HANDLERS - SPOTIFY
// // ============================================

// static void play_spotify_handler(lv_event_t* e) 
// {
//     player_state.is_playing = true;
//     strcpy(player_state.current_source, "spotify");
    
//     if (ui_pause2) 
//     {
//         lv_obj_set_style_opa(ui_pause2, LV_OPA_100, 0);
//     }
//     if (ui_play2) 
//     {
//         lv_obj_set_style_opa(ui_play2, LV_OPA_0, 0);
//     }
    
//     ESP_LOGI(TAG, "Playing Spotify: %s", spotify_tracks[player_state.current_index].name);
// }

// static void pause_spotify_handler(lv_event_t* e) 
// {
//     player_state.is_playing = false;
    
//     if (ui_play2) 
//     {
//         lv_obj_set_style_opa(ui_play2, LV_OPA_100, 0);
//     }
//     if (ui_pause2) 
//     {
//         lv_obj_set_style_opa(ui_pause2, LV_OPA_0, 0);
//     }
    
//     ESP_LOGI(TAG, "Spotify Paused");
// }

// static void slider_spotify_handler(lv_event_t* e) 
// {
//     player_state.volume = lv_slider_get_value(ui_Slider3);
//     ESP_LOGI(TAG, "Spotify Volume: %d%%", player_state.volume);
// }

// // ============================================
// // MEDIA PLAYER SCREEN SWITCHING
// // ============================================

// void switch_to_radio_screen() 
// {
//     strcpy(player_state.current_source, "radio");
//     player_state.current_index = 0;
    
//     if (radio_count > 0 && ui_radio_label) 
//     {
//         lv_label_set_text(ui_radio_label, radio_stations[0].name);
//     }
    
//     // Reset buttons visibility
//     if (ui_play) lv_obj_set_style_opa(ui_play, LV_OPA_100, 0);
//     if (ui_pause) lv_obj_set_style_opa(ui_pause, LV_OPA_0, 0);
    
//     ESP_LOGI(TAG, "Switched to Radio");
// }

// void switch_to_spotify_screen() 
// {
//     strcpy(player_state.current_source, "spotify");
//     player_state.current_index = 0;
    
//     if (spotify_count > 0 && ui_spotify_label) 
//     {
//         char display_text[200];
//         snprintf(display_text, sizeof(display_text), "%s\n%s", 
//                  spotify_tracks[0].name,
//                  spotify_tracks[0].artist);
//         lv_label_set_text(ui_spotify_label, display_text);
//     }
    
//     // Reset buttons visibility
//     if (ui_play2) lv_obj_set_style_opa(ui_play2, LV_OPA_100, 0);
//     if (ui_pause2) lv_obj_set_style_opa(ui_pause2, LV_OPA_0, 0);
    
//     ESP_LOGI(TAG, "Switched to Spotify");
// }

// // ============================================
// // MEDIA PLAYER SETUP
// // ============================================

// void media_player_setup_events() 
// {
//     // ===== RADIO CONTROLS =====
//     if (ui_play) 
//     {
//         lv_obj_add_event_cb(ui_play, play_radio_handler, LV_EVENT_CLICKED, NULL);
//     }
    
//     if (ui_pause) 
//     {
//         lv_obj_add_event_cb(ui_pause, pause_radio_handler, LV_EVENT_CLICKED, NULL);
//     }
    
//     if (ui_Slider1) 
//     {
//         lv_obj_add_event_cb(ui_Slider1, slider_radio_handler, LV_EVENT_VALUE_CHANGED, NULL);
//     }
    
//     // ===== SPOTIFY CONTROLS =====
//     if (ui_play2) 
//     {
//         lv_obj_add_event_cb(ui_play2, play_spotify_handler, LV_EVENT_CLICKED, NULL);
//     }
    
//     if (ui_pause2) 
//     {
//         lv_obj_add_event_cb(ui_pause2, pause_spotify_handler, LV_EVENT_CLICKED, NULL);
//     }
    
//     if (ui_Slider3) 
//     {
//         lv_obj_add_event_cb(ui_Slider3, slider_spotify_handler, LV_EVENT_VALUE_CHANGED, NULL);
//     }
    
//     ESP_LOGI(TAG, "Media player event handlers attached");
// }

// // ============================================
// // RADIO API - Fetch from RadioBrowser
// // ============================================

// void fetch_radio_stations() 
// {
//     // Sample radio stations (replace with API call if needed)
//     strncpy(radio_stations[0].name, "Classic FM", 99);
//     strncpy(radio_stations[0].url, "https://stream.live.icecast.org/classic", 255);
    
//     strncpy(radio_stations[1].name, "Jazz Radio", 99);
//     strncpy(radio_stations[1].url, "https://stream.live.icecast.org/jazz", 255);
    
//     strncpy(radio_stations[2].name, "Pop Hits", 99);
//     strncpy(radio_stations[2].url, "https://stream.live.icecast.org/pop", 255);
    
//     strncpy(radio_stations[3].name, "Rock Station", 99);
//     strncpy(radio_stations[3].url, "https://stream.live.icecast.org/rock", 255);
    
//     radio_count = 4;
//     ESP_LOGI(TAG, "Loaded %d radio stations", radio_count);
// }

// // ============================================
// // SPOTIFY - Sample Data
// // ============================================

// void fetch_spotify_tracks() 
// {
//     strncpy(spotify_tracks[0].name, "Blinding Lights", 99);
//     strncpy(spotify_tracks[0].artist, "The Weeknd", 99);
//     strncpy(spotify_tracks[0].url, "spotify_track_1", 255);
    
//     strncpy(spotify_tracks[1].name, "As It Was", 99);
//     strncpy(spotify_tracks[1].artist, "Harry Styles", 99);
//     strncpy(spotify_tracks[1].url, "spotify_track_2", 255);
    
//     strncpy(spotify_tracks[2].name, "Heat Waves", 99);
//     strncpy(spotify_tracks[2].artist, "Glass Animals", 99);
//     strncpy(spotify_tracks[2].url, "spotify_track_3", 255);
    
//     spotify_count = 3;
//     ESP_LOGI(TAG, "Loaded %d Spotify tracks", spotify_count);
// }

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

// ===== UI UPDATE FUNCTION =====
void updateUI_NoLock() 
{
  // ECU STATUS
  if (ecu_byte0 == 255) 
  {
    sprintf(buffer_ecu, "OFFLINE");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } 
  else if (ecu_byte0 == 1) 
  {
    sprintf(buffer_ecu, "OK");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    can_error_alert_active = false;
    lv_obj_set_style_text_opa(ui_canbus_error, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  } 
  else if (ecu_byte0 == 0) 
  {
    sprintf(buffer_ecu, "ERROR");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } 
  else 
  {
    sprintf(buffer_ecu, "UNKNOWN");
    lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
    can_error_alert_active = true;
    lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  lv_label_set_text(ui_CAR_TBD, buffer_ecu);
  lv_label_set_text(ui_CAN_TBD, buffer_ecu);

  // BATTERY STATS
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
  
  if (high_temp || low_soc || high_voltage) 
  {
    battery_alert_active = true;
    lv_obj_set_style_text_opa(ui_battery_related, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_batt_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    if (overheat) 
    {
      lv_obj_set_style_text_opa(ui_overheat, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_slow_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } 
    else 
    {
      lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    if (low_soc) 
    {
      lv_obj_set_style_text_opa(ui_low_batt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_charge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } 
    else 
    {
      lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    if (high_voltage) 
    {
      lv_obj_set_style_text_opa(ui_high_v, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_emergency_stop, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } 
    else 
    {
      lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
  } 
  else 
  {
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

  // WHEEL SPEEDS
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

  // OVERALL SPEED
  sprintf(buffer_overall_speed, "%.0f km/h", speed_kmh);
  lv_label_set_text(ui_OverallSpeed, buffer_overall_speed);

  // SPEED ALERTS
  if (speed_kmh >= 200.0f) 
  {
    speed_alert_active = true;
    lv_obj_set_style_text_opa(ui_overspeed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_slow_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_speed_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    if (speed_kmh > 245.0f)
    {
      lv_obj_set_style_text_opa(ui_max_speed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
      lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
  } 
  else 
  {
    speed_alert_active = false;
    lv_obj_set_style_text_opa(ui_overspeed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_slow_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  // WEATHER ALERTS
  if (weather_heavy_rain) 
  {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_label_set_text(ui_heavy_rain_alert, "----------------  Heavy Rain Alert");
    lv_label_set_text(ui_return_home, "Return to Home Base");
    lv_label_set_text(ui_ALERT_LABEL1, "Return to Home Base");
  }
  else if (weather_heavy_snow || weather_black_ice)
  {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_label_set_text(ui_heavy_rain_alert, "----------------  Hazard Alert");
    lv_label_set_text(ui_return_home, "Slow Down");
    lv_label_set_text(ui_ALERT_LABEL1, "Slow Down - Black Ice Detected");
  }
  else 
  {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ALERT_LABEL1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_label_set_text(ui_heavy_rain_alert, "");
    lv_label_set_text(ui_return_home, "");
    lv_label_set_text(ui_ALERT_LABEL1, "");
  }

  // UPDATE MAIN ALERT BUTTONS
  bool anyAlertActive = battery_alert_active || speed_alert_active || can_error_alert_active || weather_alert_active;

  if (anyAlertActive) 
  {
    lv_obj_set_style_bg_opa(ui_alert_red, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  } 
  else 
  {
    lv_obj_set_style_bg_opa(ui_alert_red, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

// ===== SETUP =====
void setup() 
{
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
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) 
  {
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

  // Set arc ranges
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

  // Set arcs to normal mode
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

  // Disable touch interaction on arcs
  lv_obj_clear_flag(ui_can_stats_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_car_stats_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_batt_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_fl_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_bl_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_fr_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_br_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_temp_slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_weatherslider, LV_OBJ_FLAG_CLICKABLE);

  // Set initial opacity to 0
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

  // // Pause and Play Button - Make sure they're visible and set correct opacity
  // // Radio buttons
  // if (ui_play) 
  // {
  //   lv_obj_clear_flag(ui_play, LV_OBJ_FLAG_HIDDEN);     // Ensure not hidden
  //   lv_obj_set_style_opa(ui_play, LV_OPA_100, 0);       // Play - visible (100%)
  // }
  // if (ui_pause) 
  // {
  //   lv_obj_clear_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);    // Ensure not hidden
  //   lv_obj_set_style_opa(ui_pause, LV_OPA_0, 0);        // Pause - invisible (0%)
  // }
  
  // // Spotify buttons
  // if (ui_play2) 
  // {
  //   lv_obj_clear_flag(ui_play2, LV_OBJ_FLAG_HIDDEN);    // Ensure not hidden
  //   lv_obj_set_style_opa(ui_play2, LV_OPA_100, 0);      // Play - visible (100%)
  // }
  // if (ui_pause2) 
  // {
  //   lv_obj_clear_flag(ui_pause2, LV_OBJ_FLAG_HIDDEN);   // Ensure not hidden
  //   lv_obj_set_style_opa(ui_pause2, LV_OPA_0, 0);       // Pause - invisible (0%)
  // }

  // // Initialize weather overlay to transparent
  // lv_obj_set_style_bg_opa(ui_weatherslider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  // // Initialize media player
  // fetch_radio_stations();
  // fetch_spotify_tracks();
  // media_player_setup_events();
  
  // // Set initial radio display
  // if (radio_count > 0 && ui_radio_label) 
  // {
  //   lv_label_set_text(ui_radio_label, radio_stations[0].name);
  // }
  
  // // Set initial spotify display
  // if (spotify_count > 0 && ui_spotify_label) 
  // {
  //   char display_text[200];
  //   snprintf(display_text, sizeof(display_text), "%s\n%s", 
  //            spotify_tracks[0].name,
  //            spotify_tracks[0].artist);
  //   lv_label_set_text(ui_spotify_label, display_text);
  // }
  
  // Initialize volume sliders
  if (ui_Slider1) 
  {
    lv_slider_set_value(ui_Slider1, player_state.volume, LV_ANIM_OFF);
  }
  
  if (ui_Slider3) 
  {
    lv_slider_set_value(ui_Slider3, player_state.volume, LV_ANIM_OFF);
  }

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
      updateUI_NoLock();
    }
    
    lvgl_port_unlock();
    lastUIUpdate = millis();
  }
}