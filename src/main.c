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
#ifndef PICO_STDIO_UART
#include "stdio_usb_glue.h"
#endif

#ifndef USE_PRINTF
#error This program is useless without standard input and output.
#endif

#define INIT_TASK_STACK_SIZE 256

static TaskHandle_t init_task_handle;

// USB Device Driver task
// This top level thread process all usb events and invokes callbacks
static void init_task_func(void *param)
{
    tusb_init();
#if !LIB_PICO_STDIO_UART
    stdio_usb_glue_init();
#endif
    stdio_init_all();
#if LIB_PICO_STDIO_UART
    CLI_Start();
#endif
    while (1)
    {
        tud_task();
        tud_cdc_write_flush();
    }
    vTaskDelete(NULL);
}

int main()
{
    configASSERT(xTaskCreate(init_task_func, "init", INIT_TASK_STACK_SIZE, 0, configMAX_PRIORITIES - 2, &init_task_handle) == pdPASS);
    configASSERT(init_task_handle);

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
