#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 27
extern void onewire_init();
extern uint onewire_xfer(const void * tx_buf, uint tx_size, void * rx_buf, uint rx_size);
extern void onewire_exit();
#endif
