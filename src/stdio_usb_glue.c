#include "FreeRTOS.h"
#include "pico/stdio/driver.h"
#include "semphr.h"
#include "task.h"
#include "tusb.h"
#include "stdio_cli.h"
#include "stdio_usb_glue.h"

static SemaphoreHandle_t tx_mutex, rx_mutex, write_avail_sem;
static const TickType_t xTicksToWait = pdMS_TO_TICKS(1000);

static void stdio_usb_out_chars(const char *buf, int len)
{
    int pos = 0;

    xSemaphoreTake(tx_mutex, portMAX_DELAY);
    while (pos < len) {
        if (tud_cdc_write_available())
            pos += tud_cdc_write(&buf[pos], len - pos);
        else
            xSemaphoreTake(write_avail_sem, portMAX_DELAY);
    }
    xSemaphoreTake(write_avail_sem, portMAX_DELAY); // wait for tx to complete
    xSemaphoreGive(tx_mutex);
}

static int stdio_usb_in_chars(char *buf, int len)
{
    int pos = 0;

    if (xSemaphoreTake(rx_mutex, xTicksToWait) == pdTRUE)
    {
        while (pos < len && tud_cdc_available())
        {
            const int n = tud_cdc_read(&buf[pos], len - pos);
            if (n == 0)
                break;
            pos += n;
        }
        configASSERT(xSemaphoreGive(rx_mutex) == pdTRUE);
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

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    xSemaphoreGive(write_avail_sem);
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
    configASSERT(tx_mutex = xSemaphoreCreateMutex());
    configASSERT(rx_mutex = xSemaphoreCreateMutex());
    configASSERT(write_avail_sem = xSemaphoreCreateBinary());
    stdio_set_driver_enabled(&stdio_usb, true);
#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
    configASSERT(chars_available_callback_mutex = xSemaphoreCreateMutex());
#endif
}
