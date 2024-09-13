//
// Created by Hanh Hoang on 13.9.2024.
//

#ifndef RP2040_FREERTOS_CPP_TEMPLATE_BUTTON_H
#define RP2040_FREERTOS_CPP_TEMPLATE_BUTTON_H

#define SW_0 9
#define SW_1 8
#define SW_2 7

class Button{
public:
    Button(uint pin, uint sw_nr);
    bool pressed_switch();
    uint get_sw_nr();
private:
    uint btn;
    bool pressed;
    uint sw_nr;
};
#endif //RP2040_FREERTOS_CPP_TEMPLATE_BUTTON_H
