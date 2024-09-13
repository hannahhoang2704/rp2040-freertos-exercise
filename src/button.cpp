//
// Created by Hanh Hoang on 13.9.2024.
//
#include <iostream>
#include "pico/stdlib.h"
#include "button.h"


Button::Button(uint pin, uint sw_nr): btn(pin), sw_nr(sw_nr), pressed(false){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

bool Button::pressed_switch(){
        if (!gpio_get(btn) && !pressed) {
            return (pressed = true);
        } else if (gpio_get(btn) && pressed) {
            pressed = false;
        }
        return false;
    }

uint Button::get_sw_nr(){
        return sw_nr;
}
