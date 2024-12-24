#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "crc16.h"
#include "scribble.h"
#include "putbuf.h"
#include "onewire.h"
#include "fourway.h"
#include "blheli.h"

#if 0
#define DEBUG_BUFFER(buf, size) putbuf(buf, size)
#else
#define DEBUG_BUFFER(buf, size)
#endif

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

int blheli_DeviceInitFlash(fourway_pkt_t * pkt)
{
    int ack;
    if (pkt->param_len != 1)
        ack = ACK_I_INVALID_PARAM;
    else if (pkt->param[0] != 0)
        ack = ACK_I_INVALID_CHANNEL;
    else
        ack = blheli_reset();
    
    pkt->param_len = 1;
    bzero(pkt->param, sizeof(pkt->param));
    if (ack != ACK_OK)
        return ack;

    static const char tx_buf[] = "BLHeli";
    static const size_t tx_size = sizeof(tx_buf) - 1; // minus 1 to omit the NUL terminator
    uint8_t rx_buf[16];
    static const size_t rx_size = sizeof(rx_buf);

    scribble(rx_buf, rx_size);
    onewire_putc('\000');
    const int n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    putbuf(rx_buf, n);
    if (n == 9 && strncmp(rx_buf, "471", 3) == 0 && rx_buf[8] == '0') {
        pkt->param_len = 4;
        pkt->param[0] = rx_buf[5];
        pkt->param[1] = rx_buf[4];
        pkt->param[2] = rx_buf[3];
        pkt->param[3] = rx_buf[7];
        return ACK_OK;
    }

    return ACK_D_GENERAL_ERROR;
}

int blheli_DevicePageErase(fourway_pkt_t *pkt)
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

int blheli_DeviceRead(fourway_pkt_t *pkt)
{
    const uint16_t addr = pkt->addr_msb << 8 + pkt->addr_lsb;
    int ack = blheli_set_addr(addr);

    if (ack == ACK_OK) {
        const size_t rx_size = fourway_byte_size(pkt->param[0]);
        ack = blheli_read_flash(pkt->param, rx_size);
        if (ack == ACK_OK)
            pkt->param_len = rx_size; // silently masks off bits 8-31.
    }

    if (ack != ACK_OK) {
        pkt->param_len = 1;
        pkt->param[0] = 0;
    }

    putbuf(pkt->param, fourway_param_len(pkt));
    return ack;
}

int blheli_DeviceWrite(fourway_pkt_t *pkt)
{
    int ack = ACK_OK;
    const size_t size = fourway_param_len(pkt);
    uint16_t dst = pkt->addr_msb << 8 + pkt->addr_lsb;
    const uint16_t end = dst + size;
    uint8_t *src = pkt->param;

    putbuf(src, size);
    while (dst < end) {
        uint16_t step = end - dst;
        if (step > 128)
            step = 128;
        
        ack = blheli_set_addr(dst);
        if (ack == ACK_OK)
            ack = blheli_set_buffer(src, step);
        if (ack == ACK_OK)
            ack = blheli_program_flash();
        if (ack != ACK_OK)
            break;

        src += step;
        dst += step;
    }

    pkt->param_len = 1;
    bzero(pkt->param, sizeof(pkt->param));

    return ack;
}

int blheli_DeviceReset(fourway_pkt_t *pkt)
{
    pkt->param_len = 1;
    bzero(pkt->param, sizeof(pkt->param));
    return blheli_reset();
}

int blheli_reset()
{
    static const char tx_buf[] = { 0, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    return n == 0 ? ACK_OK : ACK_D_GENERAL_ERROR;    
}

int blheli_set_addr(const uint16_t addr)
{
    const uint8_t tx_buf[] = { OP_SET_ADDR, 0, addr >> 8, addr & 255 }; // big-endian
    static const uint tx_size = sizeof(tx_buf);
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_GENERAL_ERROR;
}

int blheli_set_buffer(const void *buf, size_t size)
{
    // There is code in BLHeliBootLoad.inc that looks like it is meant to
    // handle buffers larger than 255 bytes, but AFAICS it is completely
    // borken.
    if (size > 255)
        return ACK_I_INVALID_PARAM;
    // First transfer sends the buffer size, gets no reply
    const uint8_t tx_buf[] = { OP_SET_BUFFER, 0, 0, size }; // big-endian
    static const uint tx_size = sizeof(tx_buf);
    (void) onewire_xfer(tx_buf, tx_size, NULL, 0);

    // Second transfer sends the buffer itself
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(buf, size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    if (n != 1 || rx_buf[0] != SUCCESS)
        return ACK_D_COMMAND_FAILED;
    return ACK_OK;
}

int blheli_read_flash(void *buf, size_t size)
{
    const uint8_t tx_buf[] = { OP_READ_FLASH, (size > 255 ? 0 : size) };
    static const size_t tx_size = sizeof(tx_buf);
    const size_t rx_size = size + 3; // need 3 more bytes for the CRC and error code.
    uint8_t * const rx_buf = malloc(rx_size);

    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);

    int ack = ACK_D_GENERAL_ERROR;
    if (n == rx_size && crc16(rx_buf, rx_size - 1) == 0 && rx_buf[rx_size - 1] == SUCCESS)
    {
        memcpy(buf, rx_buf, size);
        ack = ACK_OK;
    }

    free(rx_buf);
    return ack;
}

int blheli_erase_flash()
{
    const uint8_t tx_buf[] = { OP_ERASE_FLASH, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_COMMAND_FAILED;
}

int blheli_program_flash()
{
    const uint8_t tx_buf[] = { OP_PROGRAM_FLASH, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    char rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    return (n == 1 && rx_buf[0] == SUCCESS) ? ACK_OK : ACK_D_COMMAND_FAILED;
}

int blheli_ping()
{
    const uint8_t tx_buf[] = { OP_KEEP_ALIVE, 0 };
    static const size_t tx_size = sizeof(tx_buf);
    uint8_t rx_buf[16] = {0};
    static const size_t rx_size = sizeof(rx_buf);
    const size_t n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    DEBUG_BUFFER(rx_buf, n);
    return (n == 1 && rx_buf[0] == ERRORCOMMAND) ? ACK_OK : ACK_D_GENERAL_ERROR;
}
