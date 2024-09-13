//
// Created by Hanh Hoang on 13.9.2024.
//
#include "led.h"


LED::LED(uint pin): pin(pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
}

void LED::toggle_led(){
    gpio_put(pin, !gpio_get(pin));
    toggle_time = time_us_32();
}

uint32_t LED:: get_toggle_time(){
        return toggle_time;
    }

