////The rotary encoder is connected to three GPIO pins:
////• Rot_A to 10 – configure as an input without pull-up/pull-down
////• Rot_B to 11 – configure as an input without pull-up/pull-down
//// Rot_A and Rot_B have external pull-ups so they are configured as ordinary inputs
////Rot_Sw has no pull-up so we need to enable built-in pull-up
////• Rot_Sw to 12 – configure as an input with pull-up
////  Implement a program for switching a LED on/off and changing the blinking frequency. The program should work as follows:
////• Rot_Sw, the push button on the rotary encoder shaft is the on/off button. When button is pressed
////the state of LEDs is toggled. Program must require that button presses that are closer than 250 ms are ignored.
////• Rotary encoder is used to control blinking frequency of the LED. Turning the knob clockwise
////increases frequency and turning counterclockwise reduces frequency. If the LED is in OFF state
//// turning the knob has no effect. Minimum frequency is 2 Hz and maximum frequency is 200 Hz.
////When frequency is changed it must be printed
////• When LED state is toggled to ON the program must use the frequency at which it was switched off.
////You must use GPIO interrupts for detecting the encoder turns and button presses and send the button and encoder events to a event_queue.
////All queues must be registered to event_queue registry.
////Hint: create two tasks: one for receiving and filtering gpio events from the event_queue and other for blinking the LED.
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

enum rotation_direction {COUNTER_CLOCKWISE, CLOCKWISE};
enum rotation_direction rotation;

class RotaryEncoder {
public:
    RotaryEncoder(uint rot_a, uint rot_b, uint rot_sw) : rot_a(rot_a), rot_b(rot_b), rot_sw(rot_sw) {
        gpio_init(rot_a);
        gpio_init(rot_b);
        gpio_init(rot_sw);
        gpio_set_dir(rot_a, GPIO_IN);
        gpio_set_dir(rot_b, GPIO_IN);
        gpio_set_dir(rot_sw, GPIO_IN);
        gpio_pull_up(rot_sw);
    }

    static void gpio_callback(uint gpio, uint32_t events) {
        static uint32_t last_press_time = 0;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (gpio == ROT_A){
            if(gpio_get(ROT_B)){
                rotation = CLOCKWISE;
                xQueueSendToBackFromISR(event_queue, &rotation, &xHigherPriorityTaskWoken);
            }else{
                rotation = COUNTER_CLOCKWISE;
                xQueueSendToBackFromISR(event_queue, &rotation, &xHigherPriorityTaskWoken);
            }
        }else if (gpio == ROT_SW){
            uint32_t current_time = to_ms_since_boot(get_absolute_time());
            if ((current_time - last_press_time) > DEBOUNCE_SW_ROT_MS) {
                last_press_time = current_time;
                xSemaphoreGiveFromISR(xSemaphore_Rotary, &xHigherPriorityTaskWoken);
            }
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    uint rot_a;
    uint rot_b;
    uint rot_sw;
};

class LED{
public:
    LED(uint pin, int delay_ms = MAX_DELAY): pin(pin), delay_ms(delay_ms), led_state(true){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
        xLedStateMutex = xSemaphoreCreateMutex();
    }
    void blink_led(){
        if(led_state){
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    void toggle_led(){
        if(xLedStateMutex!=NULL){
            if(xSemaphoreTake(xLedStateMutex, portMAX_DELAY) == pdTRUE){
                led_state = !led_state;
                gpio_put(pin, led_state ? 1 : 0);
                xSemaphoreGive(xLedStateMutex);
            }
        }
    }

    bool is_led_on(){
        if(xLedStateMutex != NULL){
            if(xSemaphoreTake(xLedStateMutex, portMAX_DELAY) == pdTRUE){
                bool state = led_state;
                xSemaphoreGive(xLedStateMutex);
                return state;
            }
        }
    }

    int get_delay(){
        return delay_ms;
    }

    void set_delay(int new_delay){
        delay_ms = new_delay;
    }

private:
    uint pin;
    int delay_ms;
    bool led_state;
    SemaphoreHandle_t xLedStateMutex;
};

void led_task(void *param){
    LED *led = (LED *) param;
    while(true){
            led->blink_led();
    }
}
void receive_gpio_event(void *param){
    LED *led = (LED *)param;
    rotation_direction rotation_event;
    while(true) {
        if (xQueueReceive(event_queue, &rotation_event, 10) == pdTRUE) {
            if(led->is_led_on()){
                uint current_delay = led->get_delay();
                if (rotation == CLOCKWISE) {
                    if (current_delay > MIN_DELAY) {
                        current_delay -= 5;
                        led->set_delay(current_delay);
                    }
                } else if (rotation == COUNTER_CLOCKWISE) {
                    if (current_delay < MAX_DELAY) {
                        current_delay += 5;
                        led->set_delay(current_delay);
                    }
                }
                printf("Delay: %d ms, frequency is %u Hz\n", current_delay, 1000/current_delay);
            }
        }
    }
}

void toggle_task(void *param){
    LED *led = (LED *)param;
    while(true){
        if (xSemaphoreTake(xSemaphore_Rotary, portMAX_DELAY) == pdTRUE){
            std::cout << "Toggle LED" << std::endl;
            led->toggle_led();
        }else{
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

int main() {
    stdio_init_all();
    static LED led1(LED_1);
    static RotaryEncoder rotary_encoder(ROT_A, ROT_B, ROT_SW);
    event_queue = xQueueCreate(50, sizeof(rotation_direction));
    vQueueAddToRegistry(event_queue, "GPIOQueue");
    xSemaphore_Rotary = xSemaphoreCreateBinary();

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_FALL, true, &RotaryEncoder::gpio_callback);
    gpio_set_irq_enabled_with_callback(ROT_SW, GPIO_IRQ_EDGE_FALL, true, &RotaryEncoder::gpio_callback);

    xTaskCreate(led_task, "Led_task", 512, (void *)&led1, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(receive_gpio_event, "Filter_GPIO_event", 512, (void *)&led1, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(toggle_task, "Rotary_press", 512, (void *)&led1, tskIDLE_PRIORITY + 3, NULL);

    vTaskStartScheduler();

    while (1) {};
}