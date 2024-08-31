

//Implement a code lock that uses three tasks to read button presses and one task to process the button
//presses. The button presses from reader tasks must be passed to the processing task using a queue. The
//button to monitor and the queue to send to are passed through task parameters pointer.
//The processing task waits on the button queue with 5 second wait time. If a button press is received, then it
//is processed as described later. If a timeout occurs, then lock is returned to the initial state where it starts
//detecting the sequence from the beginning.
//The sequence to open the lock is 0-0-2-1-2

#include <iostream>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"

#define SW0 9
#define SW1 8
#define SW2 7

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}
const uint lock_sequence[] = {0, 0, 2, 1, 2};
const size_t sequence_length = sizeof(lock_sequence) / sizeof(lock_sequence[0]);

class Button{
public:
    Button(uint pin, uint nr): btn(pin), sw_nr(nr), pressed(false){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
    bool pressed_switch(){
        if (!gpio_get(btn) && !pressed) {
            std::cout << "Switch " << sw_nr << " pressed" << std::endl;
            return (pressed = true);
        } else if (gpio_get(btn) && pressed) {
            pressed = false;
        }
        return false;
    }
    static QueueHandle_t shared_queue;
    uint8_t get_sw_nr(){
        return sw_nr;
    }
private:
    uint btn;
    bool pressed;
    uint sw_nr;
};
QueueHandle_t Button::shared_queue = nullptr;

void read_switch(void *param){
    Button *sw = (Button *)param;
    uint sw_nr = sw->get_sw_nr();
    QueueHandle_t queue = sw->shared_queue;
    while(true){
        if(sw->pressed_switch()){
            xQueueSendToBack(queue, &sw_nr, pdMS_TO_TICKS(5000));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void process_data_input(void *param) {
    QueueHandle_t queue = (QueueHandle_t)param;
    uint received_sequence[sequence_length] = {};
    size_t index = 0;

    while (true) {
        uint button_id;
        if (xQueueReceive(queue, &button_id, pdMS_TO_TICKS(5000)) == pdTRUE) {
            std::cout << "Number received: " << button_id << std::endl;
            received_sequence[index] = button_id;
            if (received_sequence[index] == lock_sequence[index]) {
                index++;
                if (index == sequence_length) {
                    std::cout << "Lock Unlocked!" << std::endl;
                    index = 0;
                }
            } else {
                std::cout << "Wrong sequence, reset." << std::endl;
                index = 0;
            }
        } else {
            std::cout << "Timeout, reset" << std::endl;
            index = 0;
        }
    }
}

int main() {
    stdio_init_all();

    Button::shared_queue = xQueueCreate(8, sizeof(int));
    vQueueAddToRegistry(Button::shared_queue, "ButtonQueue");

    static Button sw0(SW0, 0);
    static Button sw1(SW1, 1);
    static Button sw2(SW2, 2);

    xTaskCreate(read_switch, "SwitchReader0", 256, (void *)&sw0, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(read_switch, "SwitchReader1", 256, (void *)&sw1, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(read_switch, "SwitchReader0", 256, (void *)&sw2, tskIDLE_PRIORITY + 1, NULL);

    xTaskCreate(process_data_input, "ProcessLockPw", 512, (void *)Button::shared_queue, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while (1) {};
    return 0;
}
