#include "can_data_parser.h"

// Initialize global vehicle data
VehicleData_t vehicleData = {0};

// Initialize the data structure
void can_data_init() {
    vehicleData.ecu_byte0 = 0;
    vehicleData.ecu_byte1 = 0;
    vehicleData.ecu_valid = false;
    
    vehicleData.data_0x24_valid = false;
    vehicleData.data_0x34_valid = false;
    vehicleData.data_0x35_valid = false;
    vehicleData.data_0x36_valid = false;
    vehicleData.data_0x37_valid = false;
    vehicleData.data_0x38_valid = false;
    
    vehicleData.last_update = 0;
}

// Parse CAN messages and extract data
void can_data_parse(twai_message_t &message) {
    switch(message.identifier) {
        case CAN_ID_ECU_STATUS:  // 0x08
            if (message.data_length_code >= 2) {
                vehicleData.ecu_byte0 = message.data[0];
                vehicleData.ecu_byte1 = message.data[1];

                vehicleData.ecu_valid = true;
                vehicleData.last_update = millis();
                
                Serial.printf("[0x08] ECU Status - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n", 
                             vehicleData.ecu_byte0, vehicleData.ecu_byte0,
                             vehicleData.ecu_byte1, vehicleData.ecu_byte1);
            }
            break;
            
        case CAN_ID_BATTERY_STATS:  // 0x24
            for (int i = 0; i < 8; i++) {
                if (i < message.data_length_code) {
                    vehicleData.data_0x24[i] = message.data[i];
                } else {
                    vehicleData.data_0x24[i] = 0;  // Fill remaining with zeros
                }
            }

            vehicleData.data_0x24_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x24] BATTERY Status - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d), Byte2: 0x%02X (%d), Byte3: 0x%02X (%d), Byte4: 0x%02X (%d), Byte5: 0x%02X (%d), Byte6: 0x%02X (%d), Byte7: 0x%02X (%d)\n",
                         vehicleData.data_0x24[0], vehicleData.data_0x24[0],
                         vehicleData.data_0x24[1], vehicleData.data_0x24[1],
                         vehicleData.data_0x24[2], vehicleData.data_0x24[2],
                         vehicleData.data_0x24[3], vehicleData.data_0x24[3],
                         vehicleData.data_0x24[4], vehicleData.data_0x24[4],
                         vehicleData.data_0x24[5], vehicleData.data_0x24[5],
                         vehicleData.data_0x24[6], vehicleData.data_0x24[6],
                         vehicleData.data_0x24[7], vehicleData.data_0x24[7]);
            break;
            
        case CAN_ID_FL_Wheel_Speed:  // 0x34
            for (int i = 0; i < message.data_length_code && i < 8; i++) {
                vehicleData.data_0x34[i] = message.data[i];
            }

            vehicleData.data_0x34_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x34] FRONT LEFT WHEEL Speed - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n",
                        vehicleData.data_0x34[0], vehicleData.data_0x34[0],
                        vehicleData.data_0x34[1], vehicleData.data_0x34[1]);
            break;
            
        case CAN_ID_FR_Wheel_Speed:  // 0x35
            for (int i = 0; i < message.data_length_code && i < 8; i++) {
                vehicleData.data_0x35[i] = message.data[i];
            }
            vehicleData.data_0x35_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x35] FRONT RIGHT WHEEL Speed - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n",
                        vehicleData.data_0x35[0], vehicleData.data_0x35[0],
                        vehicleData.data_0x35[1], vehicleData.data_0x35[1]);
            break;
            
        case CAN_ID_RL_Wheel_Speed:  // 0x36
            for (int i = 0; i < message.data_length_code && i < 8; i++) {
                vehicleData.data_0x36[i] = message.data[i];
            }
            vehicleData.data_0x36_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x36] REAR LEFT WHEEL Speed - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n",
                        vehicleData.data_0x36[0], vehicleData.data_0x36[0],
                        vehicleData.data_0x36[1], vehicleData.data_0x36[1]);
            break;
            
        case CAN_ID_RR_Wheel_Speed:  // 0x37
            for (int i = 0; i < message.data_length_code && i < 8; i++) {
                vehicleData.data_0x37[i] = message.data[i];
            }
            vehicleData.data_0x37_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x37] REAR RIGHT WHEEL Speed - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n",
                        vehicleData.data_0x37[0], vehicleData.data_0x37[0],
                        vehicleData.data_0x37[1], vehicleData.data_0x37[1]);
            break;
            
        case CAN_ID_VEHICLE_Speed:  // 0x38
            for (int i = 0; i < message.data_length_code && i < 8; i++) {
                vehicleData.data_0x38[i] = message.data[i];
            }
            vehicleData.data_0x38_valid = true;
            vehicleData.last_update = millis();
            
            Serial.printf("[0x38] VEHICLE Overall Speed - Byte0: 0x%02X (%d), Byte1: 0x%02X (%d)\n",
                        vehicleData.data_0x38[0], vehicleData.data_0x38[0],
                        vehicleData.data_0x38[1], vehicleData.data_0x38[1]);
            break;
            
        default:
            // Other message IDs - ignore
            break;
    }
}

// Check if data is fresh (received recently)
bool can_data_is_fresh(unsigned long timeout_ms) {
    if (!vehicleData.ecu_valid) return false;
    return (millis() - vehicleData.last_update) < timeout_ms;
}

// Convert ECU byte to arc value (0-100)
// 255 dec -> 0
// 1 dec -> 50
// 0 dec -> 100
// int16_t get_arc_value_from_ecu(uint8_t ecu_byte) {
//     int16_t arc_value;
    
//     if (ecu_byte == 255) {
//         arc_value = 0;
//     } else if (ecu_byte == 1) {
//         arc_value = 50;
//     } else if (ecu_byte == 0) {
//         arc_value = 100;
//     } else {
//         // For other values, interpolate
//         // Simple linear mapping: 255->0, 0->100
//         arc_value = 100 - (ecu_byte * 100 / 255);
//     }
    
//     return arc_value;
// }