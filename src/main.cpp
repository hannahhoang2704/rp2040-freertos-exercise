//exercise 4.2
//Implement a program with five tasks and an event group. Task 4 is a watchdog task to monitor that tasks 1
//– 3 run at least once every 30 seconds. Tasks 1 – 3 implement a loop that waits for button presses. When a
//button is pressed and released the task sets a bit in the event group, prints a debug message, and goes
//back to wait for another button press. Holding the button without releasing must block the main loop from
//running. Each task monitors one button.
//Task 4 prints “OK” and number of elapsed ticks from last “OK” when all (other) tasks have notified that they
//have run the loop. If some of the tasks does not run within 30 seconds Task 4 prints “Fail” and the number
//of the task(s) not meeting the deadline and then Task 4 suspends itself.
//Task 5 is the debug print task. It must run at a lower priority than tasks 1 – 4.
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

#define BUTTON_PRESS_BIT1 (1 << 0)
#define BUTTON_PRESS_BIT2 (1 << 1)
#define BUTTON_PRESS_BIT3 (1 << 2)
#define WATCHDOG_TIMEOUT 30 * 1000

QueueHandle_t syslog_q;
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}

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

void task1(void *param){
    Button *btn0 = (Button *)param;
    const uint32_t task_number = 1;
    while(true){
        if(btn0->button_pressed()){
            xEventGroupSetBits(xEvent_group, BUTTON_PRESS_BIT1);
            debug("[%u ms] Task %d: Button %d pressed", time_us_32()/1000, task_number, btn0->get_sw_nr());
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void task2(void *param){
    Button *btn1 = (Button *) param;
    const uint32_t task_number = 2;
    while(true){
        if(btn1->button_pressed()){
            xEventGroupSetBits(xEvent_group, BUTTON_PRESS_BIT2);
            debug("[%u ms] Task %d: Button %d pressed", time_us_32()/1000, task_number, btn1->get_sw_nr());
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void task3(void *param){
    Button *btn2 = (Button *) param;
    const uint32_t task_number = 3;
    while(true){
        if(btn2->button_pressed()){
            xEventGroupSetBits(xEvent_group, BUTTON_PRESS_BIT3);
            debug("[%u ms] Task %d: Button %d pressed", time_us_32()/1000, task_number, btn2->get_sw_nr());
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}


void watchdog_task(void *param){
    EventBits_t uxBits = xEventGroupGetBits(xEvent_group);
    char log_buffer[125];
    uint32_t last_ticks = 0;
    while(true){
        uxBits = xEventGroupWaitBits(xEvent_group, BUTTON_PRESS_BIT1 | BUTTON_PRESS_BIT2 | BUTTON_PRESS_BIT3, pdTRUE, pdTRUE, pdMS_TO_TICKS(WATCHDOG_TIMEOUT));
        if (uxBits == (BUTTON_PRESS_BIT1 | BUTTON_PRESS_BIT2 | BUTTON_PRESS_BIT3)){
            uint32_t current_ticks = time_us_32();
            debug("[%u ms] OK. Elapsed ticks: %u ms", time_us_32()/1000,(current_ticks - last_ticks)/1000, 0);
            last_ticks = current_ticks;
        }else{
            char missing_tasks[50] = {0};  // Buffer for the missing tasks list
            int index = 0;

            // Check which bits are missing and add the corresponding task number to the string
            if (!(uxBits & BUTTON_PRESS_BIT1)) {
                index += snprintf(&missing_tasks[index], sizeof(missing_tasks) - index, "1 ");
            }
            if (!(uxBits & BUTTON_PRESS_BIT2)) {
                index += snprintf(&missing_tasks[index], sizeof(missing_tasks) - index, "2 ");
            }
            if (!(uxBits & BUTTON_PRESS_BIT3)) {
                index += snprintf(&missing_tasks[index], sizeof(missing_tasks) - index, "3 ");
            }

            // Format the full message into log_buffer
            snprintf(log_buffer, sizeof(log_buffer), "[%u ms] Fail.  Suspending the task. Missing tasks: %s", time_us_32() / 1000, missing_tasks);
            debug(log_buffer, 0, 0, 0);
            vTaskSuspend(NULL);
        }
        xEventGroupClearBits(xEvent_group, BUTTON_PRESS_BIT1 | BUTTON_PRESS_BIT2 | BUTTON_PRESS_BIT3);
    }
}
int main() {
    stdio_init_all();

    static Button btn0(SW_0, 0);
    static Button btn1(SW_1, 1);
    static Button btn2(SW_2, 2);
    syslog_q = xQueueCreate(30, sizeof(debugEvent));
    vQueueAddToRegistry(syslog_q, "debug_queue");

    xEvent_group = xEventGroupCreate();

    xTaskCreate(task1, "task1", 256, (void *)&btn0, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(task2, "task2", 256, (void *)&btn1, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(task3, "task3", 256, (void *)&btn2, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(watchdog_task, "watchdog_task", 256, NULL, tskIDLE_PRIORITY+1, NULL); //task 4: watchdog
    xTaskCreate(debugTask, "debug_task", 256, NULL, tskIDLE_PRIORITY+1, NULL); //task 5: debug

    vTaskStartScheduler();

    while (1) {};
}
