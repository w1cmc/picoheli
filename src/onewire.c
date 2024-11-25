#include <hardware/dma.h>
#include <FreeRTOS.h>
#include <task.h>
#include "tx1wire.pio.h"
#include "onewire.h"

#define ONEWIRE_TASK_STACK_SIZE 256
#define ONEWIRE_NOTIFY_XFER_DONE 1
#define ONEWIRE_NOTIFY_EXIT_TASK 2

static TaskHandle_t onewire_task_handle;

static int dma_chan;

static void tx_dma_handler()
{
    dma_channel_acknowledge_irq0(dma_chan);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(onewire_task_handle, ONEWIRE_NOTIFY_XFER_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void onewire_task_func(void *arg)
{
    // We're going to use PIO to print "Hello, world!" on the same GPIO which we
    // normally attach UART0 to.
    const uint PIN_TX = 27;
    // This is the same as the default UART baud rate on Pico
    const uint SERIAL_BAUD = 19200;

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &tx1wire_program);
    tx1wire_program_init(pio, sm, offset, PIN_TX, SERIAL_BAUD);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);

    // Configure DMA to write to PIO TX FIFO
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8); // Transfer 8-bit data
    channel_config_set_read_increment(&dma_cfg, true);          // Increment read pointer
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true)); // Use TX FIFO DREQ

    static const char data[] = "BLHeli\364\175";
    static const uint DATA_SIZE = count_of(data) - 1; // minus 1 to omit the NUL terminator

    dma_channel_set_config(dma_chan, &dma_cfg, false);
    dma_channel_set_write_addr(dma_chan, &pio->txf[sm], false);

    irq_set_exclusive_handler(DMA_IRQ_0, tx_dma_handler);
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_enabled(DMA_IRQ_0, true);

    uint32_t ulNotifications;
    for (ulNotifications = 0;
        (ulNotifications & ONEWIRE_NOTIFY_EXIT_TASK) == 0;
        xTaskNotifyWait(0, ONEWIRE_NOTIFY_XFER_DONE | ONEWIRE_NOTIFY_EXIT_TASK, &ulNotifications, portMAX_DELAY))
    {     
        dma_channel_set_read_addr(dma_chan, data, false);
        dma_channel_set_trans_count(dma_chan, DATA_SIZE, true);
        vTaskDelay(pdMS_TO_TICKS(100)); // just to make it look good on the 'scope
    }

    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_cleanup(dma_chan);
    dma_channel_unclaim(dma_chan);
    dma_chan = -1;

    vTaskDelete(NULL);
}

void onewire_init()
{
    configASSERT(!onewire_task_handle);
    configASSERT(xTaskCreate(onewire_task_func, "1wire", ONEWIRE_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &onewire_task_handle) == pdPASS);
    configASSERT(onewire_task_handle);
}

void onewire_exit()
{
    configASSERT(onewire_task_handle);
    xTaskNotify(onewire_task_handle, ONEWIRE_NOTIFY_EXIT_TASK, eSetBits);
    onewire_task_handle = NULL;
}