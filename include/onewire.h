#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 27
extern void onewire_init();
extern void onewire_tx(const void * buf, uint size);
extern void onewire_exit();
#endif
