#ifndef SHADOW_FLASH
#define SHADOW_FLASH 1
extern void flash_erase(uint16_t addr);
extern void flash_read(uint16_t addr, void *buf, int size);
extern void flash_write(uint16_t addr, const void *buf, int size);
extern void flash_init();
extern void flash_ihex();
#endif
