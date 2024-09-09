//#include "PicoOsUart.h"
//#include "pico/stdlib.h"
//
//void serial_task(void *param)
//{
//    PicoOsUart u(0, 0, 1, 115200);
//    uint8_t buffer[64];
//    while (true) {
//        if(int count = u.read(buffer, 64, 30); count > 0) {
//            u.write(buffer, count);
//        }
//    }
//}
//
//int main() {
//    stdio_init_all();
//    xTaskCreate(serial_task, "serial_task", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
//    vTaskStartScheduler();
//    while(1);
//}