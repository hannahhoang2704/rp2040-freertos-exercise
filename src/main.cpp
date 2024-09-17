

//Implement a program that reads commands from the serial port using the provided interrupt driven
// FreeRTOS uart driver. The program creates two timers: one for inactivity monitoring and one for toggling
// the green led. If no characters are received in 30 seconds all the characters received so far are discarded
// and the program prints “[Inactive]”. When a character is received the inactivity timer is started/reset.
//When enter is pressed the received character are processed in a command interpreter. The commands are:
//• help – display usage instructions
//• interval <number> - set the led toggle interval (default is 5 seconds)
//• time – prints the number of seconds with 0.1s accuracy since the last led toggle
//If no valid command is found the program prints: “unknown command”.

#include <iostream>
#include <string>
#include <cstring>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "pico/stdlib.h"
#include "timers.h"
#include "PicoOsUart.h"


#define LED_1 22

#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_NR 0
#define COMMAND_TIMER_MS 30*1000
#define DEFAULT_LED_INTERVAL 5*1000

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}

const char *commands[] = {"help", "interval", "time"};
enum command_index {HELP, INTERVAL, TIME, UNKNOWN};
static TimerHandle_t xLedToggleTimer;
static TimerHandle_t xCommandTimer;

class LED{
public:
    LED(uint pin): pin(pin){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }

    void toggle_led(){
        gpio_put(pin, !gpio_get(pin));
        toggle_time = time_us_32();
    }

    uint32_t get_toggle_time(){
        return toggle_time;
    }

private:
    uint pin;
    uint32_t toggle_time = 0;
};

struct parameter{
    LED *led;
    PicoOsUart *uart;
    char command_buffer[64];
    int command_idx;
    bool received_command = false;
};

command_index get_command_index(const char *input_command, const char *commands[]){
    if(strcmp(input_command, commands[HELP]) == 0){
        return HELP;
    }
    if(strncmp(input_command, commands[INTERVAL], 8) == 0) {
        return INTERVAL;
    }
    if(strcmp(input_command, commands[TIME]) == 0){
        return TIME;
    }
    return UNKNOWN;
}

void process_command(parameter *param){
    LED *led = param->led;
    PicoOsUart *uart = param->uart;
    char *command_buffer = param->command_buffer;
    auto command_nr = get_command_index(command_buffer, commands);
    switch(command_nr){
        case HELP:{
            uart->send("help: display usage instructions\n");
            uart->send("interval <number>:  set the led toggle interval\n");
            uart->send("time:  prints the number of seconds the last led toggle\n");
            break;
        }
        case INTERVAL:{
            int interval_s = 0;
            if (sscanf(command_buffer, "interval %d", &interval_s) == 1) {
                if (xTimerChangePeriod(xLedToggleTimer, pdMS_TO_TICKS(interval_s * 1000), 0) == pdPASS) {
                    uart->send("Interval changed\n");
                } else {
                    uart->send("Interval change failed\n");
                }
            }else{
                uart->send("Invalid interval\n");
            };
            break;
        }
        case TIME:{
            uint32_t elapsed_time_us = (time_us_32() - led->get_toggle_time());
            float elapsed_time_s = (float)elapsed_time_us / 1000000.0f;
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Time since last led toggle: %.1f s\n", elapsed_time_s);
            uart->send(buffer);
            break;
        }
        case UNKNOWN:
        default:
            uart->send("Unknown command\n");
            break;
    }
    param->command_idx = 0;
    command_buffer[param->command_idx] = '\0';
}


void serial_task(void *param)
{
    parameter *p = (parameter *)param;
    PicoOsUart *uart = p->uart;
    uart->flush();
    char *command_buffer = p->command_buffer;
    int &command_idx = p->command_idx;
    uint8_t buffer[64];
    while (true) {
        if(int count = uart->read(buffer, 64, 30); count > 0) {
            xTimerReset(xCommandTimer, 0);
            uart->write(buffer, count);
            for(int i=0; i < count; ++i){
                if(buffer[i] == '\n' || buffer[i] == '\r'){
                    command_buffer[command_idx] = '\0';
                    process_command(p);
                } else {
                    command_buffer[command_idx++] = buffer[i];
                }
            }
        }
    }
}

void vLedTimerCallback(TimerHandle_t xTimer){
    parameter *param = (parameter *)pvTimerGetTimerID(xTimer);
    LED *led = param->led;
    led->toggle_led();
}

void vCommandTimerCallback(TimerHandle_t xTimer){
    parameter *param = (parameter *)pvTimerGetTimerID(xTimer);
    PicoOsUart *uart = param -> uart;
    char *command_buffer = param->command_buffer;
    param->command_idx = 0;
    command_buffer[0] = '\0';
    uart->send("[Inactive]\n");
}

int main() {
    static LED led(LED_1);
    PicoOsUart uart(UART_NR, UART_TX_PIN, UART_RX_PIN, PICO_DEFAULT_UART_BAUD_RATE);
    parameter param = {.led = &led, .uart = &uart, .command_idx = 0};
    xCommandTimer = xTimerCreate("CommandTimer", pdMS_TO_TICKS(COMMAND_TIMER_MS), pdFALSE, (void *)&param, vCommandTimerCallback);
    xLedToggleTimer = xTimerCreate("LedToggleTimers", pdMS_TO_TICKS(DEFAULT_LED_INTERVAL), pdTRUE, (void *)&param, vLedTimerCallback);

    if(xLedToggleTimer != NULL){
        xTimerStart(xLedToggleTimer, 0);
    }
    if(xCommandTimer != NULL){
        xTimerStart(xCommandTimer, 0);
    }
    xTaskCreate(serial_task, "Serial_Reader_task", 1024, (void *)&param, tskIDLE_PRIORITY+1, NULL);
    vTaskStartScheduler();

    while (1) {};
}
