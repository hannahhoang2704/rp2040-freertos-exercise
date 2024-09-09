

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
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "pico/stdlib.h"
#include "timers.h"
#include "PicoOsUart.h"

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
    void blink_led(int delay_ms){
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

    void toggle_led(){
        gpio_put(pin, !gpio_get(pin));
    }

private:
    uint pin;
};

void serial_task(void *param)
{
    PicoOsUart u(0, 0, 1, 115200);
    uint8_t buffer[64];
    while (true) {
        if(int count = u.read(buffer, 64, 30); count > 0) {
            u.write(buffer, count);
        }
    }
}

//void read_serial_port(void *param){
//    std::string text;
//    while(true){
//        if(int rv = getchar_timeout_us(0); rv != PICO_ERROR_TIMEOUT){
//            std::cout << "Received: " << (char)rv << std::endl;
//            if(rv == '\n' || rv == '\r'){
//                std::cout << "Received: " << text << std::endl;
//                text.clear();
//                xSemaphoreGive(xSemaphore);
//            } else {
//                text += static_cast<char>(rv);
//            }
//        }else{
//            vTaskDelay(pdMS_TO_TICKS(10));
//        }
//    }
//}
//void blinker_task(void *param){
//    LED *led = (LED *)param;
//    while(true){
//        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE){
//            led->blink_led(100);
//        }
//    }
//}

int main() {
    stdio_init_all();
    static LED led1(LED_1);
//    xSemaphore = xSemaphoreCreateBinary();
//    xTaskCreate(read_serial_port, "ReadSerialPort", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
//    xTaskCreate(blinker_task, "BlinkerTask", 256, (void *)&led1, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while (1) {};
    return 0;
}
