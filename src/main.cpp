//The rotary encoder is connected to three GPIO pins:
//• Rot_A to 10 – configure as an input without pull-up/pull-down
//• Rot_B to 11 – configure as an input without pull-up/pull-down
// Rot_A and Rot_B have external pull-ups so they are configured as ordinary inputs
//Rot_Sw has no pull-up so we need to enable built-in pull-up
//• Rot_Sw to 12 – configure as an input with pull-up
//  Implement a program for switching a LED on/off and changing the blinking frequency. The program should work as follows:
//• Rot_Sw, the push button on the rotary encoder shaft is the on/off button. When button is pressed
//the state of LEDs is toggled. Program must require that button presses that are closer than 250 ms are ignored.
//• Rotary encoder is used to control blinking frequency of the LED. Turning the knob clockwise
//increases frequency and turning counterclockwise reduces frequency. If the LED is in OFF state
// turning the knob has no effect. Minimum frequency is 2 Hz and maximum frequency is 200 Hz.
//When frequency is changed it must be printed
//• When LED state is toggled to ON the program must use the frequency at which it was switched off.
//You must use GPIO interrupts for detecting the encoder turns and button presses and send the button and encoder events to a event_queue.
//All queues must be registered to event_queue registry.
//Hint: create two tasks: one for receiving and filtering gpio events from the event_queue and other for blinking the LED.
#include <iostream>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "pico/stdlib.h"

#define LED_1 22
#define LED_2 21
#define LED_3 20

#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12
#define MAX_DELAY 500 // 2 Hz
#define MIN_DELAY 5 // 200 Hz
#define DEBOUNCE_SW_ROT_MS 250

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}

SemaphoreHandle_t xSemaphore;
QueueHandle_t event_queue;
struct GPIO_event{
    uint gpio;
    uint32_t events;
};

class RotaryEncoder{
public:
    RotaryEncoder(uint rot_a, uint rot_b, uint rot_sw): rot_a(rot_a), rot_b(rot_b), rot_sw(rot_sw){
        gpio_init(rot_a);
        gpio_init(rot_b);
        gpio_init(rot_sw);
        gpio_set_dir(rot_a, GPIO_IN);
        gpio_set_dir(rot_b, GPIO_IN);
        gpio_set_dir(rot_sw, GPIO_IN);
        gpio_pull_up(rot_sw);
    }
    static void gpio_callback(uint gpio, uint32_t events){
//        if(gpio == ROT_SW){
//            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//            xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
//            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//        }
//        else if(gpio == ROT_A){
//            if(gpio_get(ROT_B)){
//
//            }
//        }
            GPIO_event event = {gpio, events};
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//            xQueueSendToBackFromISR(event_queue, &event, NULL);
    }

    uint rot_a;
    uint rot_b;
    uint rot_sw;
};

class LED{
public:
    LED(uint pin, int delay_ms = MAX_DELAY): pin(pin), delay_ms(delay_ms){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    void blink_led(){
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    void toggle_led(){
        gpio_put(pin, !gpio_get(pin));
    }

    int led_state(){
        return gpio_get(pin);
    }
private:
    uint pin;
    int delay_ms;
};

void led_task(void *param){
    LED *led = (LED *)param;
    while(true){
        led->blink_led();
    }
}
void gpio_task(void *param){
    RotaryEncoder *rotary_encoder = (RotaryEncoder *)param;

}
void toggle_task(void *param){
    LED *led = (LED *)param;
    while(true){

    }
}
int main() {
    stdio_init_all();
    static LED led1(LED_1);
    static RotaryEncoder rotary_encoder(ROT_A, ROT_B, ROT_SW);
    event_queue = xQueueCreate(10, sizeof(int));
    vQueueAddToRegistry(event_queue, "GPIOQueue");
    xSemaphore = xSemaphoreCreateBinary();
    xTaskCreate(led_task, "Led_task", 512, (void *)&led1, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(gpio_task, "GPIO_Task", 512, (void *)&led1, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(toggle_task, "toggle_task", 512, (void *)&led1, tskIDLE_PRIORITY + 3, NULL);

    gpio_set_irq_enabled_with_callback(rotary_encoder.rot_a, GPIO_IRQ_EDGE_FALL, true, &RotaryEncoder::gpio_callback);
    gpio_set_irq_enabled_with_callback(rotary_encoder.rot_sw, GPIO_IRQ_EDGE_FALL, true, &RotaryEncoder::gpio_callback);

    vTaskStartScheduler();

    while (1) {};
    return 0;
}
