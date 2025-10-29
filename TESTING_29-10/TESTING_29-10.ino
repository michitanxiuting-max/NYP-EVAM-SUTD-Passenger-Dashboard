#include <lvgl.h>
#include <ui.h>

#include <Arduino.h>
#include <esp_display_panel.hpp>

#include "lvgl_v8_port.h"

#include <WiFi.h>
const char* ssid = "Michi's iPhone (2)";
const char* password = "itsMichi15";

#include <HTTPClient.h>
#include <ArduinoJson.h>

// Weather variables
unsigned long last_weather_update = 0;
String weather_condition = "Loading...";
float weather_temp = 0.0;
bool weather_heavy_rain = false;

// OpenWeatherMap API
const char* weather_api_key = "9db52acc79376ec336f7e7b4779c9e1c";
const float singapore_lat = 1.3521;
const float singapore_lon = 103.8198;

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#include "waveshare_twai_port.h"
#include "can_data_parser.h"

static bool driver_installed = false;
static bool battery_alert_was_active = false;
static bool overspeed_alert_was_active = false;
static bool can_bus_error_was_active = false;
static bool weather_alert_was_active = false;

bool battery_active = false;
bool overspeed_active = false;
bool can_bus_error_active = false;
bool weather_active = false;

// Buffers for text conversion
char buffer_ecu[16];
char buffer_batt_percent[16];
char buffer_batt_volts[16];
char buffer_batt_temp[16];
char buffer_fl_speed[16];
char buffer_fr_speed[16];
char buffer_bl_speed[16];
char buffer_br_speed[16];
char buffer_overall_speed[16];
char buffer_temp_percent[16];

// ===== WEATHER FUNCTIONS =====

// Update weather label based on condition
void update_weather_label(String condition) 
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

// Update temperature label
void update_temp_label(float temp) 
{
    sprintf(buffer_temp_percent, "%.0f°C", temp);
    lv_label_set_text(ui_TEMP_LABEL, buffer_temp_percent);
}

// Update weather overlay appearance based on condition
void update_weather_overlay()
{
    lv_color_t overlay_color;
    uint8_t overlay_opacity = 0;
    
    if (weather_condition == "Clear" || weather_condition == "Sunny") 
    {
        overlay_color = lv_color_hex(0xFFFF00);  // Light yellow
        overlay_opacity = 30;
    }
    else if (weather_condition == "Clouds" || weather_condition == "Haze") 
    {
        overlay_color = lv_color_hex(0xCCCCCC);  // Light gray
        overlay_opacity = 80;
    }
    else if (weather_condition == "Rain" || weather_condition == "Drizzle") 
    {
        overlay_color = lv_color_hex(0x555555);  // Dark gray
        overlay_opacity = 120;
    }
    else if (weather_condition == "Thunderstorm") 
    {
        overlay_color = lv_color_hex(0x333333);  // Very dark gray
        overlay_opacity = 150;
    }
    else
    {
        overlay_color = lv_color_hex(0xFFFFFF);  // White default
        overlay_opacity = 0;
    }
    
    // Apply to overlay container
    lv_obj_set_style_bg_color(ui_weather_overlay, overlay_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_weather_overlay, overlay_opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    Serial.printf("Weather overlay: %s - Opacity: %d\n", weather_condition.c_str(), overlay_opacity);
}

// Wifi Function
void wifi_setup() 
{
    delay(100);
    printf("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        printf(".");
    }
    printf("\nWiFi connected");
}

// Update weather from API
void update_weather() 
{
    if (WiFi.status() != WL_CONNECTED) 
    {
        Serial.println("WiFi not connected");
        return;
    }
    
    if (millis() - last_weather_update < 10000) return;  // Update every 10 seconds
    
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
        
        if (error) 
        {
            Serial.printf("JSON parse error: %s\n", error.c_str());
            http.end();
            return;
        }
        
        weather_condition = doc["weather"][0]["main"].as<String>();
        weather_temp = doc["main"]["temp"].as<float>();

        update_weather_label(weather_condition);
        update_temp_label(weather_temp);
        
        if (weather_condition == "Rain" || weather_condition == "Thunderstorm") 
        {
            weather_heavy_rain = true;
            weather_alert_was_active = true;
            weather_active = true;
        } 
        else 
        {
            weather_heavy_rain = false;
            weather_active = false;
        }
        
        Serial.printf("Weather Updated: %s, Temp: %.1f°C\n", weather_condition.c_str(), weather_temp);
        
        // Update weather overlay container
        lvgl_port_lock(-1);
        update_weather_overlay();
        lvgl_port_unlock();
        
        last_weather_update = millis();
    } 
    else 
    {
        Serial.printf("Weather API failed: HTTP %d\n", httpCode);
        // Set overlay to transparent on error
        lvgl_port_lock(-1);
        lv_obj_set_style_bg_opa(ui_weather_overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lvgl_port_unlock();
    }
    
    http.end();
}

// Test function with cycling weather
void test_weather_simulation() 
{
    static unsigned long last_sim = 0;
    
    if (millis() - last_sim < 10000) return;
    
    static int weather_cycle = 0;
    
    switch(weather_cycle) 
    {
        case 0:
            weather_condition = "Clear";
            weather_temp = 32.0;
            weather_heavy_rain = false;
            Serial.println("SIM: Sunny weather - 32°C");
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
    
    update_weather_label(weather_condition);
    update_temp_label(weather_temp);
    
    create_simulator_radar_image();
    display_radar_image();
    
    if (weather_heavy_rain) 
    {
        weather_alert_was_active = true;
        weather_active = true;
    } 
    else 
    {
        weather_active = false;
    }
    
    last_sim = millis();
}

// Add UI update function
void update_dashboard_ui() 
{
    lvgl_port_lock(-1);
    
    battery_active = false;
    overspeed_active = false;
    can_bus_error_active = false;
    weather_active = false;
    
    // 1. ECU STATUS 
    if (vehicleData.ecu_valid) 
    { 
        float arc_value = get_arc_value_from_ecu(vehicleData.ecu_byte0);
        
        int16_t can_value = (int16_t)arc_value;
        int16_t car_value = (int16_t)arc_value;

        lv_arc_set_value(ui_can_stats_arc, can_value);
        lv_arc_set_value(ui_car_stats_arc, car_value);

        lv_obj_set_style_text_opa(ui_canbus_error, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_wait_for, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (vehicleData.ecu_byte0 == 255) 
        {
            sprintf(buffer_ecu, "OFFLINE");
            lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x808080), LV_PART_INDICATOR);
            
            can_bus_error_was_active = true;
            can_bus_error_active = true;

            lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } 
        else if (vehicleData.ecu_byte0 == 1) 
        {
            sprintf(buffer_ecu, "OK");
            lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
        } 
        else if (vehicleData.ecu_byte0 == 0) 
        {
            sprintf(buffer_ecu, "ERROR");
            lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);

            can_bus_error_was_active = true;
            can_bus_error_active = true;

            lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } 
        else 
        {
            sprintf(buffer_ecu, "UNKNOWN");
            lv_obj_set_style_arc_color(ui_can_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(ui_car_stats_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
            can_bus_error_was_active = true;
            can_bus_error_active = true;
            
            lv_obj_set_style_text_opa(ui_canbus_error, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_wait_for, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_label_set_text(ui_CAR_TBD, buffer_ecu);
        lv_label_set_text(ui_CAN_TBD, buffer_ecu);
        
        Serial.printf("UI Update - ECU Status: Byte0=0x%02X (%d), Byte1=0x%02X (%d), Status: %s\n", 
                    vehicleData.ecu_byte0, vehicleData.ecu_byte0,
                    vehicleData.ecu_byte1, vehicleData.ecu_byte1, buffer_ecu);
    }

    // 2. BATTERY STATS
    if (vehicleData.data_0x24_valid) 
    {
        lv_arc_set_value(ui_batt_arc, vehicleData.SOC);
        
        sprintf(buffer_batt_percent, "%.0f%%", vehicleData.SOC);
        sprintf(buffer_batt_volts, "%.1fV", vehicleData.battery_voltage);
        sprintf(buffer_batt_temp, "%d°C", vehicleData.highest_cell_temp);
        
        lv_label_set_text(ui_batt_percent, buffer_batt_percent);
        lv_label_set_text(ui_batt_volts, buffer_batt_volts);
        lv_label_set_text(ui_batt_temp, buffer_batt_temp);

        float temp_value = (float)vehicleData.highest_cell_temp;
        if (temp_value < 0.0f) temp_value = 0.0f;
        if (temp_value > 100.0f) temp_value = 100.0f;
        lv_slider_set_value(ui_temp_slider, (int16_t)temp_value, LV_ANIM_ON);

        bool high_temp = (temp_value > 60.0f);
        bool overheat = (temp_value > 70.0f);
        bool low_soc = (vehicleData.SOC < 30.0f);
        bool high_voltage = (vehicleData.battery_voltage > 100.1f);
        
        if (high_temp || low_soc || high_voltage) 
        {
            battery_alert_was_active = true;
            battery_active = true;
            
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
            lv_obj_set_style_text_opa(ui_battery_related, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_overheat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_slow_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_low_batt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_high_v, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_emergency_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_batt_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            
            if (battery_alert_was_active) 
            {
                lv_obj_set_style_text_opa(ui_resolved_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_opa(ui_batt_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } 
            else 
            {
                lv_obj_set_style_text_opa(ui_resolved_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        
        Serial.printf("UI Update - Battery: %.1fV, %.1fA, %d°C, %.1f%%\n",
                    vehicleData.battery_voltage, vehicleData.battery_current, 
                    vehicleData.highest_cell_temp, vehicleData.SOC);
    }

    // 3. WHEEL Speeds
    if (vehicleData.data_0x34_valid) 
    {
        float fl_percent = (vehicleData.wheel_fl_rpm / 1966.0f) * 100.0f;
        if (fl_percent > 100.0f) fl_percent = 100.0f;
        lv_arc_set_value(ui_fl_arc, (int16_t)fl_percent);
        
        sprintf(buffer_fl_speed, "%.0f", vehicleData.wheel_fl_km);
        lv_label_set_text(ui_fl_speed, buffer_fl_speed);
        
        Serial.printf("UI Update - FL Wheel: %.1f RPM (%.1f%%), %.1f km/h\n", 
                     vehicleData.wheel_fl_rpm, fl_percent, vehicleData.wheel_fl_km);
    }
    
    if (vehicleData.data_0x35_valid) 
    {
        float fr_percent = (vehicleData.wheel_fr_rpm / 1966.0f) * 100.0f;
        if (fr_percent > 100.0f) fr_percent = 100.0f;
        lv_arc_set_value(ui_fr_arc, (int16_t)fr_percent);
        
        sprintf(buffer_fr_speed, "%.0f", vehicleData.wheel_fr_km);
        lv_label_set_text(ui_fr_speed, buffer_fr_speed);
        
        Serial.printf("UI Update - FR Wheel: %.1f RPM (%.1f%%), %.1f km/h\n", 
                     vehicleData.wheel_fr_rpm, fr_percent, vehicleData.wheel_fr_km);
    }
    
    if (vehicleData.data_0x36_valid) 
    {
        float bl_percent = (vehicleData.wheel_bl_rpm / 1966.0f) * 100.0f;
        if (bl_percent > 100.0f) bl_percent = 100.0f;
        lv_arc_set_value(ui_bl_arc, (int16_t)bl_percent);
        
        sprintf(buffer_bl_speed, "%.0f", vehicleData.wheel_bl_km);
        lv_label_set_text(ui_bl_speed, buffer_bl_speed);
        
        Serial.printf("UI Update - BL Wheel: %.1f RPM (%.1f%%), %.1f km/h\n", 
                     vehicleData.wheel_bl_rpm, bl_percent, vehicleData.wheel_bl_km);
    }

    if (vehicleData.data_0x37_valid) 
    {
        float br_percent = (vehicleData.wheel_br_rpm / 1966.0f) * 100.0f;
        if (br_percent > 100.0f) br_percent = 100.0f;
        lv_arc_set_value(ui_br_arc, (int16_t)br_percent);
        
        sprintf(buffer_br_speed, "%.0f", vehicleData.wheel_br_km);
        lv_label_set_text(ui_br_speed, buffer_br_speed);
        
        Serial.printf("UI Update - BR Wheel: %.1f RPM (%.1f%%), %.1f km/h\n", 
                     vehicleData.wheel_br_rpm, br_percent, vehicleData.wheel_br_km);
    }

    // 4. OVERALL Speed
    if (vehicleData.data_0x38_valid) 
    {
        sprintf(buffer_overall_speed, "%.0f km/h", vehicleData.speed_kmh);
        lv_label_set_text(ui_OverallSpeed, buffer_overall_speed);
        
        Serial.printf("UI Update - Speed: %.1f km/h\n", vehicleData.speed_kmh);
    }

    // Alert for overspeed
    if (vehicleData.speed_kmh > 200.0f) 
    {
        overspeed_alert_was_active = true;
        overspeed_active = true;
        
        lv_obj_set_style_text_opa(ui_overspeed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_slow_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        
        if (vehicleData.speed_kmh > 245.0f) 
        {
            lv_obj_set_style_text_opa(ui_max_speed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } 
        else 
        {
            lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_style_text_opa(ui_speed_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else 
    {
        lv_obj_set_style_text_opa(ui_overspeed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_max_speed, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_slow_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        
        if (overspeed_alert_was_active) 
        {
            lv_obj_set_style_text_opa(ui_resolved_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_speed_fault, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } 
        else 
        {
            lv_obj_set_style_text_opa(ui_resolved_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_speed_fault, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // 5. WEATHER ALERTS
    if (weather_heavy_rain) 
    {
        weather_alert_was_active = true;
        weather_active = true;
        
        lv_obj_set_style_text_opa(ui_heavy_rain_alert, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_return_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } 
    else 
    {
        lv_obj_set_style_text_opa(ui_heavy_rain_alert, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_return_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Update alert buttons based on ANY active alert
    if (battery_active || overspeed_active || can_bus_error_active || weather_active) 
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

    delay(100);
    lvgl_port_unlock();
}

void setup() 
{
    String title = "Passenger Dashboard";

    Serial.begin(115200);

    driver_installed = waveshare_twai_init();

    wifi_setup();
    printf("Connected WiFi SSID: %s\n", WiFi.SSID().c_str());

    Serial.println("Initializing board");
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) 
    {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    assert(board->begin());

    Serial.println("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    Serial.println("Creating UI");
    lvgl_port_lock(-1);
    ui_init();

    // Set arc ranges (0-100)
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

    // Disable touch interaction on arcs (make them read-only)
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

    lvgl_port_unlock();
    
    Serial.println("Setup complete - Dashboard ready");
}

void loop() 
{   
    if (!driver_installed) 
    {
        delay(1000);
        return;
    }
    
    // Receive CAN messages
    waveshare_twai_receive();

    // Update weather
    update_weather();

    // Use simulated weather (cycles every 10 seconds):
    // test_weather_simulation();

    // Update UI with parsed data
    update_dashboard_ui();
    
    delay(100);
}