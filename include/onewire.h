#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 27
extern void onewire_init();
extern void onewire_putc(int c);
extern void onewire_break();
extern size_t onewire_xfer(const void * tx_buf, size_t tx_size, void * rx_buf, size_t rx_size);
extern void onewire_exit();
#endif
