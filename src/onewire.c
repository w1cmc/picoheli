#include <hardware/dma.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include "crc16.h"
#include "onewire.pio.h"
#include "onewire.h"

#define BPS 19200
#define TX_LOOP (onewire_pgm_start + onewire_txloop_offs)

static SemaphoreHandle_t onewire_mutex;
static SemaphoreHandle_t tx_done, rx_done;

static int tx_dma_chan, crc_dma_chan, rx_dma_chan;
static uint onewire_pgm_start;

static PIO const pio = pio0;
static const uint sm = 0;

static uint16_t crc[1] __attribute__((aligned(sizeof(uint16_t))));

static inline bool tx_busy()
{
    return !pio_sm_is_tx_fifo_empty(pio, sm) || pio_sm_get_pc(pio, sm) != TX_LOOP;
}

static inline void onewire_tx_start()
{
    pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start));
}

static inline void onewire_rx_start()
{
    pio_sm_exec(pio, sm, pio_encode_jmp(onewire_pgm_start + onewire_rxstart_offs));
}

static void tx_dma_handler()
{
    dma_channel_acknowledge_irq0(crc_dma_chan);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(tx_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void rx_dma_handler()
{
    dma_channel_acknowledge_irq1(rx_dma_chan);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(rx_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void onewire_init()
{
    gpio_pull_up(ONEWIRE_PIN);
    pio_gpio_init(pio, ONEWIRE_PIN);

    onewire_pgm_start = pio_add_program(pio, &onewire_program);
    onewire_program_init(pio, sm, onewire_pgm_start, ONEWIRE_PIN, BPS);

    crc_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(crc_dma_chan);

    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_ring(&dma_cfg, false, 1);

    dma_channel_set_config(crc_dma_chan, &dma_cfg, false);
    dma_channel_set_read_addr(crc_dma_chan, crc, false);
    dma_channel_set_write_addr(crc_dma_chan, &pio->txf[sm], false);
    dma_channel_set_trans_count(crc_dma_chan, sizeof(uint16_t), false);

    tx_dma_chan = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(tx_dma_chan);

    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_chain_to(&dma_cfg, crc_dma_chan);

    dma_channel_set_config(tx_dma_chan, &dma_cfg, false);
    dma_channel_set_write_addr(tx_dma_chan, &pio->txf[sm], false);

    irq_set_exclusive_handler(DMA_IRQ_0, tx_dma_handler);
    dma_channel_set_irq0_enabled(crc_dma_chan, true);
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

    onewire_mutex = xSemaphoreCreateMutex();
    tx_done = xSemaphoreCreateBinary();
    rx_done = xSemaphoreCreateBinary();
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

void onewire_putc(int c)
{
    onewire_tx_start();
    pio_sm_put_blocking(pio, sm, c);
    while (tx_busy())
        /* wait */;
    onewire_rx_start();
}

size_t onewire_xfer(const void * tx_buf, size_t tx_size, void * rx_buf, size_t rx_size)
{
    pio_sm_clear_fifos(pio, sm);
    crc[0] = crc16(tx_buf, tx_size); // chained DMA will transmit CRC
    onewire_tx_start();
    dma_channel_set_read_addr(tx_dma_chan, tx_buf, false);
    dma_channel_set_trans_count(tx_dma_chan, tx_size, true);
    xSemaphoreTake(tx_done, portMAX_DELAY);
    while (tx_busy())
        /* wait */;

    onewire_rx_start();
    if (!rx_buf || !rx_size)
        return 0;

    dma_channel_set_write_addr(rx_dma_chan, rx_buf, false);
    dma_channel_set_trans_count(rx_dma_chan, rx_size, true);

    size_t xfer = rx_size;
    for (;;) {
        // If the IRQ sends notification, then the DMA finished ...
        if (xSemaphoreTake(rx_done, pdMS_TO_TICKS(100)) == pdTRUE)
            return rx_size;

        // ... otherwise, the DMA is still running, but is it making progress?
        const unsigned int cnt = dma_channel_hw_addr(rx_dma_chan)->transfer_count;
        if (cnt < xfer) {
            xfer = cnt;
            continue; // Yes: keep waiting.
        }

        // No: give up and go home.
        dma_channel_abort(rx_dma_chan);
        xSemaphoreTake(rx_done, portMAX_DELAY);
        break;
    }

    return rx_size - xfer;
}
