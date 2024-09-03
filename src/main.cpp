

//Part 1 – Activity indicator with a binary semaphore
//Write a program that creates two tasks: one for reading characters from the serial port and the other for
//indicating received characters on the serial port. Use a binary semaphore to notify serial port activity to the
//indicator task. Note that a single blink sequence (200 ms) takes much longer than transmission time of one
//character (0.1 ms) and only one blink after last character is allowed.
//Task 1
//Task reads characters from debug serial port using getchar_timeout_us and echoes them back to the serial
//port. When a character is received the task sends an indication (= gives the binary semaphore) to blinker task.
//Task 2
//This task blinks the led once (100 ms on, 100 ms off) when it receives activity indication (= takes the binary
//semaphore).

#include <iostream>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "pico/stdlib.h"

#define LED_1 22
#define LED_2 21
#define LED_3 20

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
    }
}

SemaphoreHandle_t xSemaphore;

class LED{
public:
    LED(uint pin): pin(pin){
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    void blink_led(int delay_ms, int count = 1){
        for (int i = 0; i < count; ++i) {
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
private:
    uint pin;
};

void read_serial_port(void *param){
    std::string text;
    while(true){
        if(int rv = getchar_timeout_us(0); rv != PICO_ERROR_TIMEOUT){
            std::cout << "Received: " << (char)rv << std::endl;
            if(rv == '\n' || rv == '\r'){
                std::cout << "Received: " << text << std::endl;
                text.clear();
            } else {
                text += static_cast<char>(rv);
            }
            xSemaphoreGive(xSemaphore);
        }else{
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
void blinker_task(void *param){
    LED *led = (LED *)param;
    while(true){
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE){
            led->blink_led(100);
        }
    }
}

int main() {
    stdio_init_all();
    static LED led1(LED_1);
    xSemaphore = xSemaphoreCreateBinary();
    xTaskCreate(read_serial_port, "ReadSerialPort", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(blinker_task, "BlinkerTask", 256, (void *)&led1, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while (1) {};
    return 0;
}
