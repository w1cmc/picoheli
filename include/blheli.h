#ifndef BLHELI_H
#define BLHELI_H
#include "fourway.h"

extern int blheli_ping();
extern int blheli_reset();
extern int blheli_set_addr(const uint16_t addr);
extern int blheli_read_flash(void * rx_buf, size_t rx_size);
extern int blheli_erase_flash();
extern int blheli_program_flash();
extern int blheli_set_buffer(const void * buf, size_t size);

extern int blheli_DeviceInitFlash(pkt_t * pkt);
extern int blheli_DeviceReset(pkt_t * pkt);
extern int blheli_DevicePageErase(pkt_t * pkt);
extern int blheli_DeviceRead(pkt_t * pkt);
extern int blheli_DeviceWrite(pkt_t *pkt);

#endif
