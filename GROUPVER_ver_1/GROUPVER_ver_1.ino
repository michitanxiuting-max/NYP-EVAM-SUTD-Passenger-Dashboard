#include <lvgl.h>
#include <ui.h>

#include <Arduino.h>
#include <esp_display_panel.hpp>

#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#include "waveshare_twai_port.h"
#include "can_data_parser.h"  // Include can data reader

static bool driver_installed = false;

// Add UI update function
void update_dashboard_ui() {
    lvgl_port_lock(-1);
    
    // Check if data is fresh (within last 2 seconds)
    if (can_data_is_fresh(2000)) {
        // Update your LVGL labels/widgets here
        // Example: if you have a label called ui_LabelECUByte0 in SquareLine Studio
        // lv_label_set_text_fmt(ui_LabelECUByte0, "0x%02X", vehicleData.ecu_byte0);
        // lv_label_set_text_fmt(ui_LabelECUByte1, "0x%02X", vehicleData.ecu_byte1);
        
        // For now, just print to serial
        Serial.printf("UI Update - ECU Status: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.ecu_byte0, vehicleData.ecu_byte1);

        Serial.printf("UI Update - BATTERY Stats: Byte0: 0x%02X (%d), Byte1: 0x%02X (%d), Byte2: 0x%02X (%d), Byte3: 0x%02X (%d), Byte4: 0x%02X (%d), Byte5: 0x%02X (%d), Byte6: 0x%02X (%d), Byte7: 0x%02X (%d)\n",
                    vehicleData.data_0x24[0], vehicleData.data_0x24[0],
                    vehicleData.data_0x24[1], vehicleData.data_0x24[1],
                    vehicleData.data_0x24[2], vehicleData.data_0x24[2],
                    vehicleData.data_0x24[3], vehicleData.data_0x24[3],
                    vehicleData.data_0x24[4], vehicleData.data_0x24[4],
                    vehicleData.data_0x24[5], vehicleData.data_0x24[5],
                    vehicleData.data_0x24[6], vehicleData.data_0x24[6],
                    vehicleData.data_0x24[7], vehicleData.data_0x24[7]);

        Serial.printf("UI Update - FRONT LEFT Wheel Speed: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.data_0x34[0], vehicleData.data_0x34[0],
                    vehicleData.data_0x34[1], vehicleData.data_0x34[1]);
        
        Serial.printf("UI Update - FRONT RIGHT Wheel Speed: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.data_0x35[0], vehicleData.data_0x35[0],
                    vehicleData.data_0x35[1], vehicleData.data_0x35[1]);

        Serial.printf("UI Update - REAR LEFT Wheel Speed: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.data_0x36[0], vehicleData.data_0x36[0],
                    vehicleData.data_0x36[1], vehicleData.data_0x36[1]);
        
        Serial.printf("UI Update - REAR RIGHT Wheel Speed: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.data_0x37[0], vehicleData.data_0x37[0],
                    vehicleData.data_0x37[1], vehicleData.data_0x37[1]);

        Serial.printf("UI Update - VEHICLE Overall Speed: Byte0=0x%02X (%d), Byte1=0x%02X (%d)\n", 
                    vehicleData.data_0x38[0], vehicleData.data_0x38[0],
                    vehicleData.data_0x38[1], vehicleData.data_0x38[1]);

    } else {
        // Data is stale or not received
        Serial.println("UI Update - No fresh EVAM data");
    }
    
    lvgl_port_unlock();
}

void setup()
{
    String title = "Passenger Dashboard";

    Serial.begin(115200);

    driver_installed = waveshare_twai_init();

    Serial.println("Initializing board");
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
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
    lvgl_port_unlock();
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
    
    // Update UI with parsed data
    update_dashboard_ui();
    
    // Small delay to prevent overwhelming the system
    delay(100);  // Update UI 10 times per second
}