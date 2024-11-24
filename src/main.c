// Standard C headers
#include <stdio.h>
#include <stdlib.h>
// Pico SDK headers
#include <hardware/gpio.h>
#include <hardware/rtc.h>
#include <pico/stdio.h>
// FreeRTOS headers
#include <FreeRTOS.h>
#include <task.h>
// TinyUSB
#include <tusb.h>
// All the rest
#include "stdio_cli.h"
#include "stdio_glue.h"

#ifndef USE_PRINTF
#error This program is useless without standard input and output.
#endif

#define TUSBD_STACK_SIZE 256

static TaskHandle_t tusbd_task_handle;

// USB Device Driver task
// This top level thread process all usb events and invokes callbacks
static void tusbd_task_func(void *param)
{
    tusb_init();
    stdio_init_all();
    CLI_Start();
    while (1)
    {
        tud_task();
        tud_cdc_write_flush();
    }
}

int main()
{
    stdio_glue_init();

    configASSERT(xTaskCreate(tusbd_task_func, "TinyUSB Device Task", TUSBD_STACK_SIZE, 0, configMAX_PRIORITIES - 2, &tusbd_task_handle) == pdPASS);
    configASSERT(tusbd_task_handle);

#if 0
    setvbuf(stdout, NULL, _IONBF, 1);  // specify that the stream should be unbuffered
    printf("\033[2J\033[H");  // Clear Screen
    stdio_flush();
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
    /* should never reach here */
    abort();
    return 0;
}
