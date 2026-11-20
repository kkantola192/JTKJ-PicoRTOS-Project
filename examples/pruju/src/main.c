
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

//global variable to store pico rotation. default 0
char picoPos = 0;
//global variable to store message to be sent, empty by default
uint8_t msg[24];

void imuTask(void *parameters);
void serialTask(char* message);

void imuTask(void *parameters) {

    float ax, ay, az, gx, gy, gz, t;

    if (init_ICM42670()==0)	{
        if (ICM42670_start_with_default_values()==0) {
        }
    } else {
        printf("failed to initialize imu");
    }

    uint8_t imuData[3];
    //update position infinitely until interrupted
    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            imuData[0] = ax;
            imuData[1] = ay;
            imuData[2] = az;
            
            //check pico position by accel sensor
            if ((imuData[1] >= 0.9) || (imuData[1] <= -0.9)){
                picoPos = '-';
            } else if ((imuData[2] >= 0.9) || (imuData[2]<=-0.9) ) {
                picoPos = '.';
            } else if ((imuData[0]) >= 0.9 || (imuData[0]<=-0.9)) {
                picoPos = ' ';
            }
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}

void printSerialTask(char* message) {
    if (message != NULL) {
        usb_serial_print(message);
    } else {
        usb_serial_print('error: null message');
    }
        }

static void btn_fxn(uint gpio, uint32_t eventMask) {
    
}

 void main() {
    init_i2c_default();
    init_sw1();
    init_sw2();
    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);


    TaskHandle_t hImuTask, hPrintSerialTask, hUSB = NULL;

//IMU task creation
    BaseType_t result = xTaskCreate(imuTask, //task
                1024, //stack size default?
                NULL, //arguments
                2, //priority
                &hImuTask //task handle
    );
    if(result != pdPASS) {
        printf("IMU Task creation failed\n");
        return 0;
    }
//serial print task creation
    result = xTaskCreate(printSerialTask, //task
                1024, //stack size default?
                NULL, //arguments
                2, //priority
                &hImuTask //task handle
    );
    if(result != pdPASS) {
        printf("Serial Print Task creation failed\n");
        return 0;
    }
 }
