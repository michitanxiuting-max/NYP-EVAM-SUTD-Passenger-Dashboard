#include <lvgl.h>
#include <ui.h>
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include "lvgl_v8_port.h"
#include "waveshare_twai_port.h"
#include "can_data_parser.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// // === MAP ===
// // map related
// #include "sutd_map_img.c"
// #define GOOGLE_API_KEY  "AIzaSyDtMfdlPTBYH60f-udaaF7CaJF9xIi3sXM"

// // Map image dimensions
// #define MAP_WIDTH   320
// #define MAP_HEIGHT  240

// // Map geolocation bounds
// #define MAP_TOP_LEFT_LAT      1.3415f
// #define MAP_TOP_LEFT_LON      103.9600f
// #define MAP_BOTTOM_RIGHT_LAT  1.3380f
// #define MAP_BOTTOM_RIGHT_LON  103.9650f

// // Update interval for geolocation (ms)
// #define UPDATE_INTERVAL_MS    20000

// // ===== GLOBALS =====  
// unsigned long lastUpdate = 0;
// float currentLat = MAP_TOP_LEFT_LAT;
// float currentLon = MAP_TOP_LEFT_LON;
// extern lv_obj_t * ui_GPS;      // map image object
// extern lv_obj_t * ui_carIcon;  // car icon image object

// ===== CONFIG =====
const char* ssid = "Michi's iPhone (2)";
const char* password = "itsMichi15";
const char* weather_api_key = "9db52acc79376ec336f7e7b4779c9e1c";

// ===== TIMING =====
unsigned long lastUIUpdate = 0;
unsigned long lastWeatherSim = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastMapUpdate = 0;

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

// // ===== FUNCTIONS =====  
// static inline lv_coord_t gps_to_x(float lon) {
//     float frac = (lon - MAP_TOP_LEFT_LON) / (MAP_BOTTOM_RIGHT_LON - MAP_TOP_LEFT_LON);
//     if (frac < 0) frac = 0;
//     if (frac > 1) frac = 1;
//     return (lv_coord_t)(frac * (MAP_WIDTH - 1));
// }

// static inline lv_coord_t gps_to_y(float lat) {
//     float frac = (MAP_TOP_LEFT_LAT - lat) / (MAP_TOP_LEFT_LAT - MAP_BOTTOM_RIGHT_LAT);
//     if (frac < 0) frac = 0;
//     if (frac > 1) frac = 1;
//     return (lv_coord_t)(frac * (MAP_HEIGHT - 1));
// }

// void animate_move_to(lv_obj_t * obj, lv_coord_t new_x, lv_coord_t new_y, uint16_t time_ms = 300) {
//     // Animate X
//     lv_anim_t a;
//     lv_anim_init(&a);
//     lv_anim_set_var(&a, obj);
//     lv_anim_set_time(&a, time_ms);
//     lv_anim_set_values(&a, lv_obj_get_x(obj), new_x);
//     lv_anim_set_exec_cb(&a, [](void* o, int32_t v){ lv_obj_set_x((lv_obj_t*)o, (lv_coord_t)v); });
//     lv_anim_start(&a);

//     // Animate Y
//     lv_anim_t b;
//     lv_anim_init(&b);
//     lv_anim_set_var(&b, obj);
//     lv_anim_set_time(&b, time_ms);
//     lv_anim_set_values(&b, lv_obj_get_y(obj), new_y);
//     lv_anim_set_exec_cb(&b, [](void* o, int32_t v){ lv_obj_set_y((lv_obj_t*)o, (lv_coord_t)v); });
//     lv_anim_start(&b);
// }

// String build_geolocate_json(const std::vector<String>& macs, const std::vector<int>& rssis) {
//     StaticJsonDocument<2048> doc;
//     JsonArray arr = doc.createNestedArray("wifiAccessPoints");
//     for (size_t i = 0; i < macs.size() && i < rssis.size(); ++i) {
//         JsonObject ap = arr.createNestedObject();
//         ap["macAddress"]     = macs[i];
//         ap["signalStrength"] = rssis[i];
//     }
//     String out;
//     serializeJson(doc, out);
//     return out;
// }

// void scan_wifi(std::vector<String>& macs, std::vector<int>& rssis, int maxAPs = 8) {
//     int n = WiFi.scanNetworks(false, true, false, 500);
//     if (n <= 0) return;
//     struct AP { int r; String mac; };
//     std::vector<AP> aps;
//     aps.reserve(n);
//     for (int i = 0; i < n && (int)aps.size() < maxAPs; i++) {
//         String mac = WiFi.BSSIDstr(i);
//         int r     = WiFi.RSSI(i);
//         aps.push_back({r, mac});
//     }
//     std::sort(aps.begin(), aps.end(), [](const AP &a, const AP &b){ return a.r > b.r; });
//     for (size_t i = 0; i < aps.size(); ++i) {
//         macs.push_back(aps[i].mac);
//         rssis.push_back(aps[i].r);
//     }
// }

// bool query_geolocation(float &outLat, float &outLon) {
//     if (WiFi.status() != WL_CONNECTED) {
//         return false;
//     }
//     String url = String("https://www.googleapis.com/geolocation/v1/geolocate?key=") + GOOGLE_API_KEY;
//     HTTPClient http;
//     http.begin(url);
//     http.addHeader("Content-Type", "application/json");

//     std::vector<String> macs;
//     std::vector<int> rssis;
//     scan_wifi(macs, rssis, 10);
//     if (macs.empty()) {
//         http.end();
//         return false;
//     }
//     String body = build_geolocate_json(macs, rssis);
//     int httpCode = http.POST(body);
//     if (httpCode == 200) {
//         String response = http.getString();
//         StaticJsonDocument<1024> doc;
//         deserializeJson(doc, response);
//         if (doc.containsKey("location")) {
//             outLat = doc["location"]["lat"] | 0.0f;
//             outLon = doc["location"]["lng"] | 0.0f;
//             http.end();
//             return true;
//         }
//     }
//     http.end();
//     return false;
// }

// void update_location_and_move_icon() {
//     float lat, lon;
//     if (!query_geolocation(lat, lon)) {
//         Serial.println("Geolocation query failed");
//         return;
//     }
//     currentLat = lat; currentLon = lon;
//     lv_coord_t x = gps_to_x(lon);
//     lv_coord_t y = gps_to_y(lat);
//     // centre icon (assuming icon size known)
//     int icon_w = lv_obj_get_width(ui_carIcon);
//     int icon_h = lv_obj_get_height(ui_carIcon);
//     lv_coord_t xs = x - icon_w/2;
//     lv_coord_t ys = y - icon_h/2;
//     animate_move_to(ui_carIcon, xs, ys, 400);
//     Serial.printf("Moved icon → x=%d y=%d lat=%.6f lon=%.6f\n", xs, ys, lat, lon);
// }

// ===== WEATHER FUNCTIONS (NO LOCKS) =====
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
  else
  {
    overlay_color = lv_color_hex(0xFFFFFF);
    overlay_opacity = 0;
  }
  
  lv_obj_set_style_bg_color(ui_weather_overlay, overlay_color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_weather_overlay, overlay_opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void testWeatherSimulation_DataOnly()  // Data update only - NO LVGL calls
{
  if (millis() - lastWeatherSim < 10000) return;
  
  static int weather_cycle = 0;
  
  switch(weather_cycle) 
  {
    case 0:
      weather_condition = "Clear";
      weather_temp = 32.0;
      weather_heavy_rain = false;
      Serial.println("SIM: Sunny - 32°C");
      break;
    case 1:
      weather_condition = "Clouds";
      weather_temp = 28.0;
      weather_heavy_rain = false;
      Serial.println("SIM: Cloudy - 28°C");
      break;
    case 2:
      weather_condition = "Rain";
      weather_temp = 24.0;
      weather_heavy_rain = true;
      Serial.println("SIM: Rainy - 24°C");
      break;
    case 3:
      weather_condition = "Thunderstorm";
      weather_temp = 22.0;
      weather_heavy_rain = true;
      Serial.println("SIM: Thunderstorm - 22°C");
      break;
  }
  
  weather_cycle = (weather_cycle + 1) % 4;
  lastWeatherSim = millis();
  // UI updates happen in main loop lock
}

void update_weather_DataOnly()  // Data update only - NO LVGL calls
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
      
      // Alert activation logic
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

  if (vehicleData.ecu_valid)
  {
    ecu_byte0 = vehicleData.ecu_byte0;
    ecu_byte1 = vehicleData.ecu_byte1;
    if (ecu_byte0 == 255) ecu_status = "OFFLINE";
    else if (ecu_byte0 == 1) ecu_status = "OK";
    else if (ecu_byte0 == 0) ecu_status = "ERROR";
    else ecu_status = "UNKNOWN";
  }

  if (vehicleData.data_0x24_valid)
  {
    SOC = vehicleData.SOC;
    battery_voltage = vehicleData.battery_voltage;
    highest_cell_temp = vehicleData.highest_cell_temp;
    battery_current = vehicleData.battery_current;
  }

  if (vehicleData.data_0x34_valid)
  {
    wheel_fl_rpm = vehicleData.wheel_fl_rpm;
    wheel_fl_km = vehicleData.wheel_fl_km;
  }

  if (vehicleData.data_0x35_valid)
  {
    wheel_fr_rpm = vehicleData.wheel_fr_rpm;
    wheel_fr_km = vehicleData.wheel_fr_km;
  }

  if (vehicleData.data_0x36_valid)
  {
    wheel_bl_rpm = vehicleData.wheel_bl_rpm;
    wheel_bl_km = vehicleData.wheel_bl_km;
  }

  if (vehicleData.data_0x37_valid)
  {
    wheel_br_rpm = vehicleData.wheel_br_rpm;
    wheel_br_km = vehicleData.wheel_br_km;
  }

  if (vehicleData.data_0x38_valid)
  {
    speed_kmh = vehicleData.speed_kmh;
  }
}

// ===== UI UPDATE FUNCTION (NO LOCK - called inside lock) =====
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
  } 
  else 
  {
    lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
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
  } 
  else 
  {
    lv_obj_set_style_bg_opa(ui_alert_red, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alert_red5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
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

  // Initialize weather overlay to transparent
  lv_obj_set_style_bg_opa(ui_weather_overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  // // Set static map on UI
  //   lv_img_set_src(ui_GPS, &sutd_map_img);
  //   lv_obj_set_size(ui_GPS, MAP_WIDTH, MAP_HEIGHT);
  //   lv_obj_set_pos(ui_GPS, 232, 4);  // adjust if needed

  lvgl_port_unlock();

  printf("Dashboard initialized\n");
}

// ===== MAIN LOOP - SINGLE LOCK FOR ALL LVGL OPERATIONS =====
void loop() 
{
  // 1. Process CAN data (no lock needed)
  processCANData();

  // 2. Update weather data only - no LVGL calls
  // testWeatherSimulation_DataOnly();
  // OR use real API
  update_weather_DataOnly();

  // Print every 500ms instead of every update
  if (millis() - lastSerialPrint >= 500)
  {
    Serial.printf("Speed: %.1f | SOC: %.0f%% | Temp: %d | Update count: %d\n",
                  speed_kmh, SOC, highest_cell_temp, millis());
    lastSerialPrint = millis();
  }

  // if (millis() - lastUpdate > UPDATE_INTERVAL_MS) {
  //       update_location_and_move_icon();
  //       lastUpdate = millis();
  // }

  // 3. ONE BIG LOCK - all LVGL updates happen here every 100ms
  if (millis() - lastUIUpdate >= 50)
  {
    lvgl_port_lock(-1);  // ONE lock for everything
    
    // Update weather display
    updateWeatherLabel_NoLock(weather_condition);
    update_temp_label_NoLock(weather_temp);
    updateWeatherOverlay_NoLock();
    
    // Update all UI elements
    updateUI_NoLock();
    
    lvgl_port_unlock();  // ONE unlock
    lastUIUpdate = millis();
  }
}