// CRT
#include <stdio.h>
// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>
// Raspberry Pi Pico SDK
#include <pico/stdlib.h>
// all the rest
#include "command.h"
#include "task_config.h"
#include "stdio_cli.h"

#define CLI_TASK_STACK_SIZE 1024
#define CLI_NOTIFY_CHARS_AVAIL 1
#define CLI_NOTIFY_EXIT_TASK 2

static TaskHandle_t cli_task_handle;

static void callback(void *unused) {
    if (cli_task_handle == 0)
        return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(cli_task_handle, CLI_NOTIFY_CHARS_AVAIL, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// stdioTask - the function which handles input
static void cli_task_func(void *unused) {
    (void) unused;

    // get notified when there are input characters available
    stdio_set_chars_available_callback(callback, xTaskGetCurrentTaskHandle());

    fputs("CLI> ", stdout);
    fflush(stdout);

    for (;;) {
        uint32_t ulNotifications;
        if (xTaskNotifyWait(0, CLI_NOTIFY_CHARS_AVAIL | CLI_NOTIFY_EXIT_TASK, &ulNotifications, portMAX_DELAY) == pdTRUE) {
            if (ulNotifications & CLI_NOTIFY_EXIT_TASK) {
                stdio_set_chars_available_callback(NULL, NULL);
                vTaskDelete(NULL);            
            }
            if (ulNotifications & CLI_NOTIFY_CHARS_AVAIL) {
                /* Get characters from the terminal */
                int c;
                while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
                    input_scan(c);
            }
        }
    }
}

void CLI_Start()
{
    if (cli_task_handle)
        return; /* already started */

    configASSERT(xTaskCreate(cli_task_func, "CLI", CLI_TASK_STACK_SIZE, 0, configMAX_PRIORITIES - 2, &cli_task_handle) == pdPASS);
    configASSERT(cli_task_handle);
}

void CLI_Stop()
{
    if (!cli_task_handle)
        return; /* not started */

    xTaskNotify(cli_task_handle, CLI_NOTIFY_EXIT_TASK, eSetBits);
    cli_task_handle = NULL;
}