#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "blheli.h"
#include "ihex.h"
#include "shadow_flash.h"

static uint8_t flash[8192];

void flash_erase(uint16_t addr)
{
    if (addr < sizeof(flash))
        memset(&flash[addr & ~511], 255, 512);
}

void flash_write(uint16_t addr, const void *buf, int size)
{
    const uint8_t * src = buf;
    const uint8_t * const src_end = &src[size];
    uint8_t * dst = &flash[addr];
    uint8_t * const dst_end = &flash[sizeof(flash)];
    while (src < src_end && dst < dst_end)
        *dst++ &= *src++;
}

void flash_read(uint16_t addr, void *buf, int size)
{
    if (addr + size > sizeof(flash))
        size = sizeof(flash) - addr;
    memcpy(buf, &flash[addr], size);
}

void flash_init()
{
    static const uint16_t step = 256;
    uint16_t addr = 0;
    int ack = ACK_OK;

    while (addr < sizeof(flash)) {
        ack = blheli_set_addr(addr);
        if (ack == ACK_OK)
            ack = blheli_read_flash(&flash[addr], step);
        if (ack != ACK_OK)
            break;
        addr += step;
    }

    if (ack != ACK_OK)
        puts("flash_init failed");
}

void flash_ihex()
{
    ihexify(0, flash, sizeof(flash));
}