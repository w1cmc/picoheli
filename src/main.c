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
#include "onewire.h"
#include "stdio_cli.h"
#ifndef PICO_STDIO_UART
#include "stdio_usb_glue.h"
#endif

#ifndef USE_PRINTF
#error This program is useless without standard input and output.
#endif

#define INIT_TASK_STACK_SIZE 256

static TaskHandle_t init_task_handle, blinky_task_handle;

static void blinky_task_func(void *param)
{
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    bool flip = false;
    while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250)) == pdFALSE) {
        flip = !flip;
        gpio_put(25, flip);
    }

    vTaskDelete(NULL);
}

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
    onewire_init();

    configASSERT(xTaskCreate(init_task_func, "init", INIT_TASK_STACK_SIZE, 0, configMAX_PRIORITIES - 2, &init_task_handle) == pdPASS);
    configASSERT(init_task_handle);

    configASSERT(xTaskCreate(blinky_task_func, "blinky", 128, NULL, configMAX_PRIORITIES - 2, &blinky_task_handle));
    configASSERT(blinky_task_handle);

#if 0
    setvbuf(stdout, NULL, _IONBF, 1);  // specify that the stream should be unbuffered
    printf("\033[2J\033[H");  // Clear Screen
    stdio_flush();
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
    /* should never reach here */
    abort();
    onewire_exit();
    return 0;
}
