//
// Created by Hanh Hoang on 13.9.2024.
//

#ifndef RP2040_FREERTOS_CPP_TEMPLATE_LED_H
#define RP2040_FREERTOS_CPP_TEMPLATE_LED_H
#include "pico/stdlib.h"
#define LED_1 22

class LED{
public:
    LED(uint pin);
    void toggle_led();
    uint32_t get_toggle_time();
private:
    uint pin;
    uint32_t toggle_time = 0;
};

#endif //RP2040_FREERTOS_CPP_TEMPLATE_LED_H
