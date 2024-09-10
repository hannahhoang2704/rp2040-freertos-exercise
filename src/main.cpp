

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

const std::string commands[] = {"help", "interval", "time"};
enum command_index {HELP, INTERVAL, TIME, UNKNOWN};
static std::string command_buffer;
static TimerHandle_t xLedToggleTimer = NULL;

class LED{
public:
    LED(uint pin): pin(pin){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }

    void toggle_led(){
        gpio_put(pin, !gpio_get(pin));
        toggle_time = read_runtime_ctr();
    }

    uint32_t get_toggle_time(){
        return toggle_time;
    }

private:
    uint pin;
    uint32_t toggle_time = 0;
};
static LED led(LED_1);

command_index get_command_index(const std::string input_command, const std::string (&commands)[3]){
    if(input_command.compare(commands[HELP]) == 0){
        return HELP;
    }
    if(input_command.find(commands[INTERVAL]) != std::string::npos){
        return INTERVAL;
    }
    if(input_command.compare(commands[TIME]) == 0){
        return TIME;
    }
    return UNKNOWN;
}

void process_command(PicoOsUart *uart, LED *led){
    command_index command_idx = get_command_index(command_buffer, commands);
    switch(command_idx){
        case HELP:{
            uart->send("help: display usage instructions\n");
            uart->send("interval <number>:  set the led toggle interval\n");
            uart->send("time:  prints the number of seconds the last led toggle\n");
            break;
        }
        case INTERVAL:{
            int interval_s = 0;
            if (sscanf(command_buffer.c_str(), "interval %d", &interval_s) == 1){
                std::cout << "Interval set to " << interval_s << " s\n";
                xTimerChangePeriod(xLedToggleTimer, pdMS_TO_TICKS(interval_s * 1000), 0);
                uart->send("New interval changed\n");
            } else {
                uart->send("Invalid interval\n");
            }
            break;
        }
        case TIME:{
            uint32_t elapsed_time = (read_runtime_ctr() - led->get_toggle_time())/10000;
            uart->send("Time since last led toggle: " + std::to_string(elapsed_time) + " s\n");
            break;
        }
        case UNKNOWN:
        default:
            uart->send("Unknown command\n");
            break;
    }
    command_buffer.clear();
}

//void serial_task(void *param)
//{
//    LED *led = (LED *)param;
//    PicoOsUart uart(UART_NR, UART_TX_PIN, UART_RX_PIN, PICO_DEFAULT_UART_BAUD_RATE);
//    uint8_t buffer[64];
//    while (true) {
//        if(int count = uart.read(buffer, 64, 500); count > 0) {
//            uart.write(buffer, count);
//            for(int i = 0; i < count; ++i){
//                std::cout << "receive " << buffer[i] << std::endl;
//                if(buffer[i] == '\n' || buffer[i] == '\r'){
//                    process_command(&uart, led);
//                } else {
//                    command_buffer += static_cast<char>(buffer[i]);
//                }
//            }
//        }else{
//            vTaskDelay(pdMS_TO_TICKS(10));
//        }
//    }
//}

void serial_task(void *param)
{
    LED *led = (LED *)param;
    PicoOsUart uart(UART_NR, UART_TX_PIN, UART_RX_PIN, PICO_DEFAULT_UART_BAUD_RATE);
    uint8_t buffer[64];
    while (true) {
        if(int count = uart.read(buffer, 64, 30); count > 0) {
            for(int i = 0; i < count; ++i){
                std::cout << "receive " << buffer[i] << std::endl;
                if(buffer[i] == '\n' || buffer[i] == '\r'){
                    process_command(&uart, led);
                } else {
                    command_buffer += static_cast<char>(buffer[i]);
                }
            }
        }
    }
}

void vLedTimerCallback(TimerHandle_t xTimer){
    led.toggle_led();
}

void vCommandTimerCallback(TimerHandle_t xTimer){
    PicoOsUart uart(UART_NR, UART_TX_PIN, UART_RX_PIN, PICO_DEFAULT_UART_BAUD_RATE);
    command_buffer.clear();
    uart.send("[Inactive]\n");
}

int main() {
    TimerHandle_t xCommandTimer = xTimerCreate("CommandTimer", pdMS_TO_TICKS(COMMAND_TIMER_MS), pdTRUE, (void *)0, vCommandTimerCallback);
    TimerHandle_t xLedToggleTimer = xTimerCreate("LedToggleTimers", pdMS_TO_TICKS(DEFAULT_LED_INTERVAL), pdTRUE, (void *)0, vLedTimerCallback);

    if(xLedToggleTimer != NULL){
        xTimerStart(xLedToggleTimer, 0);
    }
    if(xCommandTimer != NULL){
        xTimerStart(xCommandTimer, 0);
    }
//    serial_task();
    xTaskCreate(serial_task, "Serial_Reader_task", 1024, (void *)&led, tskIDLE_PRIORITY+1, NULL);
    vTaskStartScheduler();

    while (1) {};
}
