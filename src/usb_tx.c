#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <tusb.h>
#include "usb_tx.h"

static SemaphoreHandle_t txAvailMutex, txLockMutex;

void usb_tx_init()
{
    txAvailMutex = xSemaphoreCreateBinary();
    txLockMutex = xSemaphoreCreateBinary();
    xSemaphoreGive(txLockMutex);
}

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    xSemaphoreGive(txAvailMutex);
}

void usb_tx_buf(const void *buf, size_t size)
{
    const uint8_t * pos = buf;
    const uint8_t * const end = &pos[size];

    xSemaphoreTake(txLockMutex, portMAX_DELAY);
    while (pos < end) {
        const size_t need = end - pos;
        const size_t avail = tud_cdc_write_available();
        const size_t write_size = need > avail ? avail : need;
        if (write_size > 0)
            pos += tud_cdc_write(pos, write_size);
        else
            xSemaphoreTake(txAvailMutex, pdMS_TO_TICKS(50));
    }
    
    tud_cdc_write_flush();
    xSemaphoreGive(txLockMutex);
}
