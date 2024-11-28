#include <hardware/dma.h>
#include <FreeRTOS.h>
#include <task.h>
#include "onewire.pio.h"
#include "onewire.h"

#define ONEWIRE_NOTIFY_XFER_DONE 1
#define BPS 19200
#define TX_PULL (onewire_pgm_start + onewire_tx_pull)

static TaskHandle_t onewire_task_handle;

static int tx_dma_chan;
static uint onewire_pgm_start;

static PIO const pio = pio0;
static const uint sm = 0;

static inline void onewire_tx_start()
{
   pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start + onewire_tx_offset));
}

static inline void onewire_rx_start()
{
   pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start + onewire_rx_offset));
}

static void tx_dma_handler()
{
    dma_channel_acknowledge_irq0(tx_dma_chan);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(onewire_task_handle, ONEWIRE_NOTIFY_XFER_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void onewire_init()
{
    gpio_pull_up(ONEWIRE_PIN);
    pio_gpio_init(pio, ONEWIRE_PIN);

    onewire_pgm_start = pio_add_program(pio, &onewire_program);
    onewire_program_init(pio, sm, onewire_pgm_start, ONEWIRE_PIN, BPS);

    tx_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(tx_dma_chan);

    // Configure DMA to write to PIO TX FIFO
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8); // Transfer 8-bit data
    channel_config_set_read_increment(&dma_cfg, true);          // Increment read pointer
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true)); // Use TX FIFO DREQ

    dma_channel_set_config(tx_dma_chan, &dma_cfg, false);
    dma_channel_set_write_addr(tx_dma_chan, &pio->txf[sm], false);

    irq_set_exclusive_handler(DMA_IRQ_0, tx_dma_handler);
    dma_channel_set_irq0_enabled(tx_dma_chan, true);
    irq_set_enabled(DMA_IRQ_0, true);

    onewire_rx_start();
}

void onewire_exit()
{
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_cleanup(tx_dma_chan);
    dma_channel_unclaim(tx_dma_chan);
    tx_dma_chan = -1;
}

void onewire_tx(const void * buf, uint size)
{
    onewire_task_handle = xTaskGetCurrentTaskHandle();
    onewire_tx_start();
    dma_channel_set_read_addr(tx_dma_chan, buf, false);
    dma_channel_set_trans_count(tx_dma_chan, size, true);
    uint32_t ulNotifications = 0;
    xTaskNotifyWait(0, ONEWIRE_NOTIFY_XFER_DONE, &ulNotifications, portMAX_DELAY);
    while (!pio_sm_is_tx_fifo_empty(pio, sm))
        /* wait */;
    while (pio_sm_get_pc(pio, sm) != TX_PULL)
        /* wait */;
    onewire_rx_start();
}
