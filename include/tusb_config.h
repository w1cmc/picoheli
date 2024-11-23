#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE // Use device mode only

// CDC class configuration
#define CFG_TUD_CDC 1                         // Enable CDC device support (1 instance)
#define CFG_TUD_MSC 0                         // Disable MSC (Mass Storage Class)
#define CFG_TUD_HID 0                         // Disable HID (Human Interface Device)
#define CFG_TUD_MIDI 0                        // Disable MIDI class
#define CFG_TUD_VENDOR 0                      // Disable Vendor class

// CDC FIFO sizes (optional tuning)
#define CFG_TUD_CDC_RX_BUFSIZE 64             // Size of the receive buffer
#define CFG_TUD_CDC_TX_BUFSIZE 64             // Size of the transmit buffer

#endif /* _TUSB_CONFIG_H_ */
