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
const uint8_t correct_sequence[] = {0, 0, 2, 1, 2};
const size_t sequence_length = sizeof(correct_sequence) / sizeof(correct_sequence[0]);

struct sw_params {
    QueueHandle_t comm_q;
    uint pin;
    uint8_t sw_nr;
    bool pressed = false;
};

bool pressed_switch(sw_params * sw){
    if (!gpio_get(sw->pin) && !sw->pressed) {
        return (sw->pressed = true);
    } else if (gpio_get(sw->pin) && sw->pressed) {
        sw->pressed = false;
    }
    return false;
}

void switch_reader(void *param) {
    sw_params *tpr = (sw_params *)param;
    const uint pin = tpr->pin;
    const uint sw_nr = tpr->sw_nr;
    QueueHandle_t queue = tpr->comm_q;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

    while(true) {
        if(pressed_switch(tpr)){
            xQueueSendToBack(queue, &sw_nr, pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void process_data_input(void *param) {
    QueueHandle_t queue = (QueueHandle_t)param;
    uint8_t received_sequence[sequence_length] = {};
    size_t index = 0;

    while (true) {
        uint8_t button_id;
        if (xQueueReceive(queue, &button_id, pdMS_TO_TICKS(5000)) == pdTRUE) {
            std::cout << "Received button: " << (int)button_id << std::endl;
            received_sequence[index] = button_id;
            if (received_sequence[index] == correct_sequence[index]) {
                index++;
                if (index == sequence_length) {
                    std::cout << "Lock Unlocked!" << std::endl;
                    index = 0;
                }
            } else {
                std::cout << "Incorrect sequence, resetting." << std::endl;
                index = 0;
            }
        } else {
            std::cout << "Timeout, resetting sequence." << std::endl;
            index = 0;
        }
    }
}

int main() {
    stdio_init_all();

    QueueHandle_t button_queue = xQueueCreate(10, sizeof(uint8_t));

    static sw_params sw0_params = {.comm_q = button_queue, .pin = SW0, .sw_nr = 0};
    static sw_params sw1_params = {.comm_q = button_queue, .pin = SW1, .sw_nr = 1};
    static sw_params sw2_params = {.comm_q = button_queue, .pin = SW2, .sw_nr = 2};

    xTaskCreate(switch_reader, "SwitchReader0", 256, (void *)&sw0_params, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(switch_reader, "SwitchReader1", 256, (void *)&sw1_params, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(switch_reader, "SwitchReader2", 256, (void *)&sw2_params, tskIDLE_PRIORITY + 1, NULL);

    xTaskCreate(process_data_input, "ProcessLockPw", 512, (void *)button_queue, tskIDLE_PRIORITY + 2, NULL);

    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    while (1) {};
    return 0;
}
