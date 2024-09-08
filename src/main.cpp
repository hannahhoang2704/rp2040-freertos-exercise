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

SemaphoreHandle_t xSemaphore_Rotary;
QueueHandle_t event_queue;
QueueHandle_t delay_value_queue;
enum rotation_direction {COUNTER_CLOCKWISE, CLOCKWISE};
enum rotation_direction rotation;

struct rotary_encoder{
    uint rot_a;
    uint rot_b;
    uint rot_sw;
};

class LED{
public:
    LED(uint pin, int delay_ms = MAX_DELAY): pin(pin), delay_ms(delay_ms), led_state(false){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    void blink_led(int delay){
        if(led_state){
            delay_ms = delay;
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    void toggle_led(){
        led_state = !led_state;
        gpio_put(pin, led_state ? 1 : 0);
    }

    bool is_led_on(){
        return led_state;
    }

    int get_delay(){
        return delay_ms;
    }

private:
    uint pin;
    int delay_ms;
    bool led_state;
};
static void gpio_callback(uint gpio, uint32_t events){
    if (gpio == ROT_A){
        if(gpio_get(ROT_B)){
            //clockwise
            rotation = CLOCKWISE;
            xQueueSendToBackFromISR(event_queue, &rotation, NULL);
        }else{
            //counter clockwise
            rotation = COUNTER_CLOCKWISE;
            xQueueSendToBackFromISR(event_queue, &rotation, NULL);
        }
    }else if (gpio == ROT_SW){
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xSemaphore_Rotary, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void led_task(void *param){
    LED *led = (LED *)param;
    int delay_ms;
    while(true){
        if(led->is_led_on() && xQueueReceive(delay_value_queue, &delay_ms, pdMS_TO_TICKS(100)) == pdTRUE){
            led->blink_led(delay_ms);
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
void receive_gpio_event(void *param){
    rotary_encoder *rotary = (rotary_encoder *) param;
    int delay_ms;
    gpio_set_irq_enabled_with_callback(rotary->rot_a, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    while(true) {
        if (xQueueReceiveFromISR(event_queue, &rotation, NULL) == pdTRUE) {
            if (rotation == CLOCKWISE) {
                std::cout << "Counter clockwise" << std::endl;
                if (delay_ms > MIN_DELAY) {
                    delay_ms -= 5;
                    xQueueSendToBack(delay_value_queue, &delay_ms, pdMS_TO_TICKS(100));
                }
            } else if (rotation == COUNTER_CLOCKWISE) {
                std::cout << "Counter anti-clockwise" << std::endl;
                if (delay_ms < MAX_DELAY) {
                    delay_ms += 5;
                    xQueueSendToBack(delay_value_queue, &delay_ms, pdMS_TO_TICKS(100));
                }
            }
        }
    }
}

void toggle_task(void *param){
    rotary_encoder *rotary = (rotary_encoder *) param;
    gpio_set_irq_enabled_with_callback(rotary->rot_sw, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    while(true){
        if (xSemaphoreTake(xSemaphore_Rotary, pdMS_TO_TICKS(DEBOUNCE_SW_ROT_MS)) == pdTRUE){
            //toggle led
            LED *led = (LED *)param;
            led->toggle_led();
        }
    }
}

int main() {
    stdio_init_all();
    static LED led1(LED_1);
    rotary_encoder rotary_encoder = {ROT_A, ROT_B, ROT_SW};
    event_queue = xQueueCreate(50, sizeof(rotation_direction));
    delay_value_queue = xQueueCreate(50, sizeof(int));
    vQueueAddToRegistry(event_queue, "GPIOQueue");
    vQueueAddToRegistry(delay_value_queue, "Delay_ms_Queue");
    xSemaphore_Rotary = xSemaphoreCreateBinary();
    xTaskCreate(led_task, "Led_task", 512, (void *)&led1, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(receive_gpio_event, "Filter_GPIO_event", 512, (void *)&rotary_encoder, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(toggle_task, "Rotary_press", 512, (void *)&rotary_encoder, tskIDLE_PRIORITY + 3, NULL);

    vTaskStartScheduler();

    while (1) {};
}
