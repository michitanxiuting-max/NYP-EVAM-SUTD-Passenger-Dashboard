// can_data_parser.h
#ifndef __CAN_DATA_PARSER_H
#define __CAN_DATA_PARSER_H

#include <Arduino.h>
#include "driver/twai.h"

// CAN Message IDs
#define CAN_ID_ECU_STATUS       0x00000008
#define CAN_ID_BATTERY_STATS    0x00000024
#define CAN_ID_FL_Wheel_Speed   0x00000034
#define CAN_ID_FR_Wheel_Speed   0x00000035
#define CAN_ID_RL_Wheel_Speed   0x00000036
#define CAN_ID_RR_Wheel_Speed   0x00000037
#define CAN_ID_VEHICLE_Speed    0x00000038

// Vehicle data structure
typedef struct {
    // 0x08 - ECU Status
    uint8_t ecu_byte0;
    uint8_t ecu_byte1;
    bool ecu_valid;
    
    // 0x24 data
    uint8_t data_0x24[8];
    bool data_0x24_valid;
    
    // 0x34 data
    uint8_t data_0x34[8];
    bool data_0x34_valid;
    
    // 0x35 data
    uint8_t data_0x35[8];
    bool data_0x35_valid;
    
    // 0x36 data
    uint8_t data_0x36[8];
    bool data_0x36_valid;
    
    // 0x37 data
    uint8_t data_0x37[8];
    bool data_0x37_valid;
    
    // 0x38 data
    uint8_t data_0x38[8];
    bool data_0x38_valid;
    
    unsigned long last_update;
} VehicleData_t;

// Global vehicle data
extern VehicleData_t vehicleData;

// Function declarations
void can_data_init();
void can_data_parse(twai_message_t &message);
bool can_data_is_fresh(unsigned long timeout_ms);
int16_t get_arc_value_from_ecu(uint8_t ecu_byte);

#endif // __CAN_DATA_PARSER_H