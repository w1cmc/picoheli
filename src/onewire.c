#include <hardware/dma.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "onewire.pio.h"
#include "onewire.h"

#define BPS 19200
#define TX_PULL (onewire_pgm_start + onewire_txloop_offs)

static TaskHandle_t onewire_task_handle;

static int tx_dma_chan, rx_dma_chan;
static uint onewire_pgm_start;

static PIO const pio = pio0;
static const uint sm = 0;

static inline void onewire_tx_start()
{
    pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start));
}

static inline void onewire_rx_start()
{
    pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start + onewire_rxstart_offs));
}

static void dma_irq_handler(int irqn, int dma_chan)
{
    dma_irqn_acknowledge_channel(irqn, dma_chan);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(onewire_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}
static void tx_dma_handler()
{
    dma_irq_handler(0, tx_dma_chan);
}

static void rx_dma_handler()
{
    dma_irq_handler(1, rx_dma_chan);
}

void onewire_init()
{
    gpio_pull_up(ONEWIRE_PIN);
    pio_gpio_init(pio, ONEWIRE_PIN);

    onewire_pgm_start = pio_add_program(pio, &onewire_program);
    onewire_program_init(pio, sm, onewire_pgm_start, ONEWIRE_PIN, BPS);

    tx_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(tx_dma_chan);

    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);

    dma_channel_set_config(tx_dma_chan, &dma_cfg, false);
    dma_channel_set_write_addr(tx_dma_chan, &pio->txf[sm], false);

    irq_set_exclusive_handler(DMA_IRQ_0, tx_dma_handler);
    dma_channel_set_irq0_enabled(tx_dma_chan, true);
    irq_set_enabled(DMA_IRQ_0, true);

    rx_dma_chan = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(rx_dma_chan);

    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);

    dma_channel_set_config(rx_dma_chan, &dma_cfg, false);
    dma_channel_set_read_addr(rx_dma_chan, &pio->rxf[sm], false);

    irq_set_exclusive_handler(DMA_IRQ_1, rx_dma_handler);
    dma_channel_set_irq1_enabled(rx_dma_chan, true);
    irq_set_enabled(DMA_IRQ_1, true);

    // put the pin in receive/input mode
    onewire_rx_start();
}

void onewire_exit()
{
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_cleanup(tx_dma_chan);
    dma_channel_unclaim(tx_dma_chan);
    tx_dma_chan = -1;

    irq_set_enabled(DMA_IRQ_1, false);
    dma_channel_cleanup(rx_dma_chan);
    dma_channel_unclaim(rx_dma_chan);
    rx_dma_chan = -1;
}

uint onewire_xfer(const void * tx_buf, uint tx_size, void * rx_buf, uint rx_size)
{
    onewire_task_handle = xTaskGetCurrentTaskHandle();
    onewire_tx_start();
    dma_channel_set_read_addr(tx_dma_chan, tx_buf, false);
    dma_channel_set_trans_count(tx_dma_chan, tx_size, true);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (!pio_sm_is_tx_fifo_empty(pio, sm) || pio_sm_get_pc(pio, sm) != TX_PULL)
        /* wait */;

    onewire_rx_start();
    if (!rx_buf || !rx_size)
        return 0;

    dma_channel_set_write_addr(rx_dma_chan, rx_buf, false);
    dma_channel_set_trans_count(rx_dma_chan, rx_size, true);

    uint xfer = rx_size;
    for (;;) {
        // If the IRQ sends notification, then the DMA finished
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)))
            return rx_size;

        // The DMA is still running, but is it making progress?
        const uint cnt = dma_channel_hw_addr(rx_dma_chan)->transfer_count;
        if (cnt < xfer) {
            xfer = cnt;
            continue; // yes: keep waiting.
        }

        // no: give up and go home.
        dma_channel_abort(rx_dma_chan);
        break;
    }

    return rx_size - xfer;
}
