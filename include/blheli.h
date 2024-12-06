#ifndef BLHELI_H
#define BLHELI_H
#include "fourway.h"

extern int blheli_ping();
extern int blheli_set_addr(uint16_t addr);
extern int blheli_read_flash(void * rx_buf, size_t rx_size);

extern int blheli_DeviceInitFlash(pkt_t * pkt);
extern int blheli_DeviceReset(pkt_t * pkt);
extern int blheli_DeviceRead(pkt_t * pkt);

#endif
