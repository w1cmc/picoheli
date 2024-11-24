#include "FreeRTOS.h"
#include "pico/stdio/driver.h"
#include "semphr.h"
#include "task.h"
#include "tusb.h"
#include "stdio_cli.h"
#include "stdio_usb_glue.h"

static SemaphoreHandle_t stdio_tud_cdc_mutex;
static const TickType_t xTicksToWait = pdMS_TO_TICKS(1000);

static void stdio_usb_out_chars(const char *buf, int len)
{
    int pos = 0;

    if (xSemaphoreTake(stdio_tud_cdc_mutex, xTicksToWait) == pdTRUE)
    {
        while (pos < len && tud_cdc_write_available())
        {
            const int n = tud_cdc_write(&buf[pos], len - pos);
            if (n == 0)
                break;
            pos += n;
        }
        configASSERT(xSemaphoreGive(stdio_tud_cdc_mutex) == pdTRUE);
    }

    // kinda borken API: this function should return the number of chars written
    // return pos ? pos : PICO_ERROR_NO_DATA;
}

static int stdio_usb_in_chars(char *buf, int len)
{
    int pos = 0;

    if (xSemaphoreTake(stdio_tud_cdc_mutex, xTicksToWait) == pdTRUE)
    {
        while (pos < len && tud_cdc_available())
        {
            const int n = tud_cdc_read(&buf[pos], len - pos);
            if (n == 0)
                break;
            pos += n;
        }
        configASSERT(xSemaphoreGive(stdio_tud_cdc_mutex) == pdTRUE);
    }

    return pos ? pos : PICO_ERROR_NO_DATA;
}

#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
static void (*callback)(void *) = NULL;
static void *param = NULL;
static SemaphoreHandle_t chars_available_callback_mutex;

static void stdio_usb_set_chars_available_callback(void (*cb)(void *), void *p)
{
    if (xSemaphoreTake(chars_available_callback_mutex, xTicksToWait) == pdTRUE)
    {
        callback = cb;
        param = p;
        configASSERT(xSemaphoreGive(chars_available_callback_mutex) == pdTRUE);
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    if (xSemaphoreTake(chars_available_callback_mutex, xTicksToWait) == pdTRUE)
    {
        if (callback)
            callback(param);
        configASSERT(xSemaphoreGive(chars_available_callback_mutex) == pdTRUE);
    }
}
#endif

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    static bool prev_dtr;

    if (dtr && !prev_dtr) {
        CLI_Start();
    }
    else if (prev_dtr && !dtr) {
        CLI_Stop();
    }

    prev_dtr = dtr;
}

static stdio_driver_t stdio_usb = {
    .out_chars = stdio_usb_out_chars,
    .in_chars = stdio_usb_in_chars,
#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
    .set_chars_available_callback = stdio_usb_set_chars_available_callback,
#endif
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF
#endif
};

void stdio_usb_glue_init()
{
    configASSERT(stdio_tud_cdc_mutex = xSemaphoreCreateMutex());
    stdio_set_driver_enabled(&stdio_usb, true);
#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
    configASSERT(chars_available_callback_mutex = xSemaphoreCreateMutex());
#endif
}
