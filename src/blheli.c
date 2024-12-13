#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include "crc16.h"
#include "onewire.h"
#include "fourway.h"
#include "blheli.h"

#define BOOT_START 0x1c00

enum {
    SUCCESS = 0x30,
    ERRORVERIFY = 0xC0,
    ERRORCOMMAND = 0xC1,
    ERRORCRC = 0xC2,
    ERRORPROG = 0xC5
};

// BLHeli boot loader commands
enum {
    OP_RESTART = 0,
    OP_PROGRAM_FLASH = 1,
    OP_ERASE_FLASH = 2,
    OP_READ_FLASH = 3,
    OP_SET_ADDR = 255,
    OP_SET_BUFFER = 254,
    OP_KEEP_ALIVE = 253,
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

static void scribble(void * buf, size_t size)
{
    static const uint8_t deadbeef[] = { 0xef, 0xbe, 0xad, 0xde };
    const uint8_t * src = deadbeef;
    const uint8_t * const src_end = &src[sizeof(deadbeef)];
    uint8_t * dst = buf;
    uint8_t * const dst_end = &dst[size];
    while (dst < dst_end) {
        *dst++ = *src++;
        if (src == src_end)
            src = deadbeef;
    }
}

int blheli_DeviceInitFlash(pkt_t * pkt)
{
    static const char tx_buf[] = "BLHeli";
    static const size_t tx_size = sizeof(tx_buf) - 1; // minus 1 to omit the NUL terminator
    uint8_t rx_buf[16];
    static const size_t rx_size = sizeof(rx_buf);

    scribble(rx_buf, rx_size);
    onewire_putc('\000');
    const int n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    if (n == 9 && strncmp(rx_buf, "471", 3) == 0 && rx_buf[8] == '0') {
        pkt->param_len = 4;
        pkt->param[0] = rx_buf[5];
        pkt->param[1] = rx_buf[4];
        pkt->param[2] = rx_buf[3];
        pkt->param[3] = rx_buf[7];
        return ACK_OK;
    }

    pkt->param_len = 1;
    pkt->param[0] = 0;
    return ACK_D_GENERAL_ERROR;
}

int blheli_DevicePageErase(pkt_t *pkt)
{
    const uint16_t page = pkt->param[0];
    const uint16_t addr = page << 9; // 512 byte pages
    if (BOOT_START < addr)
        return ACK_I_INVALID_PARAM; // trying to overwrite the bootloader
    int ack = blheli_set_addr(addr);
    if (ack == ACK_OK)
        ack = blheli_erase_flash();
    return ack;
}

int blheli_DeviceRead(pkt_t *pkt)
{
    const uint16_t addr = pkt->addr_msb << 8 + pkt->addr_lsb;
    int ack = blheli_set_addr(addr);

    if (ack == ACK_OK) {
        const size_t rx_size = byte_size(pkt->param[0]);
        ack = blheli_read_flash(pkt->param, rx_size);
        if (ack == ACK_OK)
            pkt->param_len = rx_size; // silently masks off bits 8-31.
    }

    if (ack != ACK_OK) {
        pkt->param_len = 1;
        pkt->param[0] = 0;
    }

    return ack;
}

int blheli_DeviceWrite(pkt_t *pkt)
{
    const uint16_t addr = pkt->addr_msb << 8 + pkt->addr_lsb;
    int ack = blheli_set_addr(addr);
    if (ack == ACK_OK) {
        ack = blheli_set_buffer(pkt->param, param_len(pkt));
        if (ack == ACK_OK)
            ack = blheli_program_flash();
    }

    pkt->param_len = 1;
    memset(pkt->param, 0, sizeof(pkt->param)); // we miss you, bzero(3)
    return ack;
}

int blheli_DeviceReset(pkt_t *pkt)
{
    static const char tx_buf[] = { 0, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return n == 0 ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_set_addr(const uint16_t addr)
{
    const uint8_t tx_buf[] = { 0xff, 0, addr >> 8, addr & 255 }; // big-endian
    static const uint tx_size = sizeof(tx_buf);
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_set_buffer(const void *buf, size_t size)
{
    // First transfer sends the buffer size
    const uint8_t tx_buf[] = { 0xfe, 0, size >> 8, size & 255 }; // big-endian
    static const uint tx_size = sizeof(tx_buf);
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    if (n != 1 || rx_buf[0] != SUCCESS)
        return ACK_D_COMMAND_FAILED;

    // Second transfer sends the buffer itself
    n = onewire_xfer(buf, size, rx_buf, rx_size);
    putbuf(rx_buf, n);
    if (n != 1 || rx_buf[0] != SUCCESS)
        return ACK_D_COMMAND_FAILED;
    return ACK_OK;
}

int blheli_read_flash(void *rx_buf, size_t rx_size)
{
    const uint8_t tx_buf[] = { OP_READ_FLASH, (rx_size > 255 ? 0 : rx_size) };
    static const size_t tx_size = sizeof(tx_buf);
    // A little sketchy to read 3 more bytes, but we need the CRC and error code.
    // Since rx_buf is always the param array of a pkt_t, there is enough space.
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size + 3);
    puts(__func__);
    putbuf(rx_buf, n);
    uint8_t * const end = (uint8_t *) rx_buf + n;
    if (n == rx_size + 3 &&
        crc16(rx_buf, rx_size + 2) == 0 &&
        end[-1] == SUCCESS)
        return ACK_OK;
    return ACK_D_GENERAL_ERROR;
}

int blheli_erase_flash()
{
    const uint8_t tx_buf[] = { OP_ERASE_FLASH, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_COMMAND_FAILED;
}

int blheli_program_flash()
{
    const uint8_t tx_buf[] = { OP_PROGRAM_FLASH, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_COMMAND_FAILED;
}

int blheli_ping()
{
    const uint8_t tx_buf[] = { OP_KEEP_ALIVE, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    puts(__func__);
    putbuf(rx_buf, n);
    return (n == 1 && rx_buf[0] == ERRORCOMMAND) ? ACK_OK : ACK_D_GENERAL_ERROR;
}
