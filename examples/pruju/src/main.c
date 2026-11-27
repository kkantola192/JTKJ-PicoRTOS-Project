
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <tusb.h>
#include "usbSerialDebug/helper.h"
#include <tkjhat/sdk.h>

#define DEF_STACK_SIZE 1024

//global variable to store pico rotation. default newline
char picoPos = '\n';
//global variable to store message to be sent, empty by default, 255 elements
uint8_t msg[255];
//global variables to save button states
static volatile uint8_t char_save, send_msg;
//global variable to store index of message array, iterated in imuTask, reset in serialTask
uint8_t msgIndex = 0;

void imuTask(void *parameters);
void serialTask(void* parameters);

//task to handle IMU data and translating it to an appropriate character
void imuTask(void *parameters) {

    //variables for IMU data, only ax, ay, az are used
    float ax, ay, az, gx, gy, gz, t;

    if (init_ICM42670()==0)	{
        if (ICM42670_start_with_default_values()==0) {
        }
    } else {
        printf("failed to initialize imu");
    }

    //update position infinitely until interrupted
    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            
            //check pico position by accel sensor
            //max accel per axis is ~1, 0.9 is plenty high threshold to prevent issues
            //check both extremes of accel data so that messages can be sent regardless of direction, only care about orientation
            if (ay>=0.9 || ay<=-0.9) {
                picoPos = '-';
            } else if (az>=0.9 || az<=-0.9) {
                picoPos = '.';
            } else if (ax >= 0.9 || ax <= -0.9) {
                picoPos = ' ';
            }
            //handles checking user input to save current character to msg array
            if (char_save) {
                //sets a \n to the end of the message for usb_serial_print, would probably more sensible to handle in printTask?
                //doesn't check whether or not the message array is full, so this could cause issues with >254 character messages
                msg[msgIndex+1] = '\n';
                msg[msgIndex] = picoPos;
                msgIndex++;
                char_save = false;
            }
        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}
//Task to send message over serial via USB
void serialTask(void* parameters) {
//Define message array pointer
    uint8_t* message = msg;
    while(1) {
        //Send message 
        if (send_msg) {
                //check for a null message pointer, shouldn't ever happen
                //but I guess it'll be fine to check regardless
            if (message != NULL) {
                //send message over serial, clear msg related variables to prepare for next message
            usb_serial_print(message);
            send_msg = false;
            msgIndex = 0;
            memset(msg, 0, sizeof(msg));
            } else if (message == NULL) {
                //send error message over serial, once again, shouldn't really happen but eh, whatever
            usb_serial_print("NULL message\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }    
}

static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

static void btn_fxn(uint gpio, uint32_t eventMask) {
    //check which button is pressed, change state accordingly, also gives priority to character save over msg send
    //states are set false when input is handled
    if (gpio==SW1_PIN) {
        char_save = true;
          //usb_serial_print("TESTI 1\n");
    } else if (gpio==SW2_PIN) {
        send_msg = true;
         //usb_serial_print("TESTI 2 \n");
    }   
}

 int main(void) {
    stdio_init_all();
    init_i2c_default();
    sleep_ms(300);    //wait for initialization
    init_sw1();
    init_sw2();
    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled_with_callback(SW2_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    TaskHandle_t hImuTask, hserialTask, hUSB = NULL;

//IMU task creation
    BaseType_t result = xTaskCreate(imuTask, //task
                "imu", //name
                DEF_STACK_SIZE, //stack size
                NULL, //arguments
                2, //priority
                &hImuTask //task handle
    );
    if(result != pdPASS) {
        printf("IMU Task creation failed\n");
        return 0;
    }
//serial print task creation
    result = xTaskCreate(serialTask, //task
                "serialPrint", //task name, actual task is serialTask, was renamed at some point and didn't think to change this
                DEF_STACK_SIZE, //stack size
                NULL, //arguments
                3, //priority
                &hserialTask //task handle
    );
    if(result != pdPASS) {
        printf("Serial Print Task creation failed\n");
        return 0;
    }
//USB task creation, highest priority
    result = xTaskCreate(usbTask, "usb", DEF_STACK_SIZE, NULL, 4, &hUSB);
    if(result != pdPASS) {
        printf("USB Task creation failed\n");
        return 0;
    }

    tusb_init();
    usb_serial_init();
    vTaskStartScheduler();
 }
