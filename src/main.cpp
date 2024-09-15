
//exercise 4.1
//Implement a program with four tasks and an event group. Task 1 waits for user to press a button. When the
//button is pressed task 1 sets bit 0 of the event group. Tasks 2 and 3 wait on the event bit with an infinite
//timeout. When the bit is set the tasks start running their main loops. In the main loop each task prints task
//number and number of elapsed ticks since the last print at random interval that is between 1 – 2 seconds.
//Task 4 is debug print task. Tasks 1 – 3 must run at higher priority than the debug task. The tasks must use
//the debug function described above for printing.

#include <iostream>
#include <string>
#include <cstring>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "pico/stdlib.h"
#include "timers.h"
#include "button.h"

#define BUTTON_PRESS_BIT (1 << 0)

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}

QueueHandle_t syslog_q;
EventGroupHandle_t xEvent_group;

void debug(const char *format, uint32_t d1, uint32_t d2, uint32_t d3);

struct debugEvent{
    const char *format;
    uint32_t data[3];
    float timestamp;
};

void debugTask(void *pvParameters){
    char buffer[125];
    debugEvent event;
    while(true){
        if(xQueueReceive(syslog_q, &event, portMAX_DELAY) == pdTRUE){
            char formatted_message[100];
            snprintf(formatted_message, sizeof (formatted_message), event.format, event.data[0], event.data[1], event.data[2]);
            snprintf(buffer, sizeof(buffer), "[%.2f s]\t%s", event.timestamp, formatted_message);
            std::cout << buffer << std::endl;
        }
    }
}

void debug(const char *format, uint32_t d1, uint32_t d2, uint32_t d3){
    uint32_t current_timestamp_us = time_us_32();
    float current_timestamp_s = current_timestamp_us / 1000000.0f;
    debugEvent event = {.format = format, .data = {d1, d2, d3}, .timestamp = current_timestamp_s};
    xQueueSendToBack(syslog_q, &event, 0);
}

void button_task(void *pvParameters){
    Button *btn = (Button *)pvParameters;
    while(true){
        if(btn->pressed_switch()){
            debug("Button pressed on pin %d", btn->get_sw_nr(), 0, 0);
            xEventGroupSetBits(xEvent_group, BUTTON_PRESS_BIT);
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void task2(void *param){
    const uint32_t task_number = 2;
    const TickType_t max_delay = pdMS_TO_TICKS(2000); // Max delay for random interval
    const TickType_t min_delay = pdMS_TO_TICKS(1000); // Min delay for random interval
    EventBits_t uxBits;
    srand(xTaskGetTickCount() + task_number);
    while (true) {
        uxBits = xEventGroupWaitBits(xEvent_group, BUTTON_PRESS_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        if (uxBits & BUTTON_PRESS_BIT) {
            TickType_t delay = (rand() % (max_delay - min_delay)) + min_delay;
            vTaskDelay(delay);
            uint32_t elapsed_ticks = xTaskGetTickCount();
            debug("Task %d. Elapsed ticks: %u ms", task_number, elapsed_ticks, 0);
        }
    }
}

void task3(void *param){
    const uint32_t task_number = 3;
    const TickType_t max_delay = pdMS_TO_TICKS(2000); // Max delay for random interval
    const TickType_t min_delay = pdMS_TO_TICKS(1000); // Min delay for random interval
    EventBits_t uxBits;
    srand(xTaskGetTickCount() + task_number);
    while (true) {
        uxBits = xEventGroupWaitBits(xEvent_group, BUTTON_PRESS_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        if (uxBits & BUTTON_PRESS_BIT) {
            TickType_t delay = (rand() % (max_delay - min_delay)) + min_delay;
            vTaskDelay(delay);
            uint32_t elapsed_ticks = xTaskGetTickCount();
            debug("Task %d. Elapsed ticks: %u ms", task_number, elapsed_ticks, 0);
        }
    }
}

int main() {
    stdio_init_all();
    static Button btn(SW_0, 0);
    syslog_q = xQueueCreate(30, sizeof(debugEvent));
    vQueueAddToRegistry(syslog_q, "debug_queue");

    xEvent_group = xEventGroupCreate();

    xTaskCreate(button_task, "button_task", 256, (void *)&btn, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(task2, "task2", 256, NULL, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(task3, "task3", 256, NULL, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(debugTask, "debug_task", 256, NULL, tskIDLE_PRIORITY+1, NULL);

    vTaskStartScheduler();

    while (1) {};
}
