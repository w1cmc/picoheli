#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include "onewire.h"
#include "fourway.h"
#include "blheli.h"

enum {
    SUCCESS = 0x30,
    ERRORVERIFY = 0xC0,
    ERRORCOMMAND = 0xC1,
    ERRORCRC = 0xC2,
    ERRORPROG = 0xC5
};

static void putbuf(const uint8_t * const buf, size_t size)
{
    int i;

    if (size == 0) {
        puts("no data received");
        return;
    }

    for (i=0; i<size; ++i) {
        printf("%02x", buf[i]);
        putchar((i & 7) == 7 ? '\n' : ' ');
    }
    
    if (i & 7)
        putchar('\n');
}

int blheli_DeviceInitFlash(pkt_t * pkt)
{
    static const char tx_buf[] = "BLHeli";
    static const size_t tx_size = sizeof(tx_buf) - 1; // minus 1 to omit the NUL terminator
    uint8_t rx_buf[16];
    static const size_t rx_size = sizeof(rx_buf);

    onewire_putc('\000');
    const int n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    if (n == 9 && strncmp(rx_buf, "471", 3) == 0 && rx_buf[8] == '0') {
        pkt->param_len = 3;
        pkt->param[0] = rx_buf[4];
        pkt->param[1] = rx_buf[5];
        pkt->param[2] = rx_buf[3];
        return ACK_OK;
    }

    pkt->param_len = 1;
    pkt->param[0] = 0;
    return ACK_D_GENERAL_ERROR;
}

int blheli_DeviceReset(pkt_t *pkt)
{
    static const char tx_buf[] = { 0, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[1] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return n == 0 ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_set_addr(uint16_t addr)
{
    const char tx_buf[] = { 0xff, 0, (addr >> 8), addr & 255 };
    static const uint tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_read_flash(void *rx_buf, size_t rx_size)
{
    const uint8_t tx_buf[] = { 3, rx_size };
    static const size_t tx_size = sizeof(tx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return n == rx_size ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_ping()
{
    const uint8_t tx_buf[] = { 0xfd, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    uint8_t rx_buf[1] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == ERRORCOMMAND) ? ACK_OK : ACK_D_GENERAL_ERROR;
}