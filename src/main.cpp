
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
#define MAX_DELAY 2000
#define MIN_DELAY 1000

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
};


void debugTask(void *pvParameters){
    char buffer[125];
    debugEvent event;
    while(true){
        if(xQueueReceive(syslog_q, &event, portMAX_DELAY) == pdTRUE){
            snprintf(buffer, sizeof(buffer), event.format, event.data[0], event.data[1], event.data[2]);
            std::cout << buffer << std::endl;
        }
    }
}


void debug(const char *format, uint32_t d1, uint32_t d2, uint32_t d3){
    debugEvent event = {.format = format, .data = {d1, d2, d3}};
    xQueueSendToBack(syslog_q, &event, 0);
}


uint32_t get_random_delay(uint32_t min_delay, uint32_t max_delay){
    srand(time_us_32());
    return (rand() % (max_delay - min_delay)) + min_delay;
}k

void button_task(void *pvParameters){
    Button *btn = (Button *)pvParameters;
    const uint32_t task_number = 1;
    while(!btn->pressed_switch()){
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    debug("Button pressed on pin %d", btn->get_sw_nr(), 0, 0);
    xEventGroupSetBits(xEvent_group, BUTTON_PRESS_BIT);
    TickType_t last_tick = xTaskGetTickCount();
    while(true){
        uint32_t delay = get_random_delay(MIN_DELAY, MAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(delay));
        TickType_t current_tick = xTaskGetTickCount();
        debug("[%u ms] Task %d. Elapsed ticks since last print: %u ms", time_us_32()/1000,task_number, current_tick - last_tick);
        last_tick = current_tick;
    }
}


void task2(void *param){
    const uint32_t task_number = 2;
    EventBits_t uxBits = xEventGroupWaitBits(xEvent_group, BUTTON_PRESS_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    TickType_t last_tick = xTaskGetTickCount();
    while ((uxBits & BUTTON_PRESS_BIT) != 0) {
        uint32_t delay = get_random_delay(MIN_DELAY, MAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(delay));
        uint32_t current_timestamps = time_us_32()/1000;
        TickType_t current_tick = xTaskGetTickCount();
        debug("[%u ms] Task %d. Elapsed ticks since last print: %u ms", current_timestamps, task_number, current_tick-last_tick);
        last_tick = current_tick;
    }
}



void task3(void *param){
    const uint32_t task_number = 3;
    EventBits_t  uxBits = xEventGroupWaitBits(xEvent_group, BUTTON_PRESS_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    TickType_t last_tick = xTaskGetTickCount();
    while ((uxBits & BUTTON_PRESS_BIT) != 0) {
        uint32_t delay = get_random_delay(MIN_DELAY, MAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(delay));
        uint32_t current_timestamps = time_us_32()/1000;
        TickType_t current_tick = xTaskGetTickCount();
        debug("[%u ms] Task %d. Elapsed ticks since last print: %u ms", current_timestamps, task_number, current_tick-last_tick);
        last_tick = current_tick;
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
