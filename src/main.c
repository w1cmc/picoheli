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
#include "fourway.h"
#include "stdio_cli.h"
#ifndef PICO_STDIO_UART
#include "stdio_usb_glue.h"
#endif

#ifndef USE_PRINTF
#error This program is useless without standard input and output.
#endif

#define TUD_TASK_STACK_SIZE 1024
#define BLINKY_TASK_STACK_SIZE 128

static TaskHandle_t tud_task_handle;
static TaskHandle_t blinky_task_handle;

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

static void tud_task_func(void *param)
{
    puts("TinyUSB Device task started");
    tusb_init();
#if !LIB_PICO_STDIO_UART
    stdio_usb_glue_init();
#endif

    while (1)
    {
        tud_task();
    }

    vTaskDelete(NULL);
}

int main()
{
    onewire_init();
    fourway_init();

    configASSERT(xTaskCreate(blinky_task_func, "blinky", BLINKY_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &blinky_task_handle));
    configASSERT(blinky_task_handle);

    configASSERT(xTaskCreate(tud_task_func, "TinyUSB Device", TUD_TASK_STACK_SIZE, 0, configMAX_PRIORITIES - 2, &tud_task_handle) == pdPASS);
    configASSERT(tud_task_handle);

#if LIB_PICO_STDIO_UART
    CLI_Start();
    stdio_init_all();
#endif
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
