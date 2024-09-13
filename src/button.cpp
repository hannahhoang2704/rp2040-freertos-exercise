//
// Created by Hanh Hoang on 13.9.2024.
//
#include <iostream>
#include "pico/stdlib.h"
#include "button.h"
#include "FreeRTOS.h"
#include "task.h"


Button::Button(uint pin, uint sw_nr): btn(pin), sw_nr(sw_nr), pressed(false){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN); //set pin 0
        gpio_pull_up(pin);
    }

//bool Button::button_pressed(){
//    if (!gpio_get(btn) && !pressed) {
//        return (pressed = true);
////            pressed = true;
//    } else if (gpio_get(btn) && pressed) {
//        pressed = false;
////            return true;
//    }
//    return false;
//}

bool Button::button_pressed() {
    if (!gpio_get(btn) && !pressed) {
        pressed = true;
        vTaskDelay(pdMS_TO_TICKS(50));  // Debounce
        while (!gpio_get(btn)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return true;
    }
    if (gpio_get(btn) && pressed) {
        pressed = false;
    }
    return false;
}


uint Button::get_sw_nr(){
        return sw_nr;
}
