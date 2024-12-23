typedef struct {
    uint8_t preamble[2];
    uint8_t direction;
    uint8_t size;
    uint8_t cmd;
    uint8_t data[256]; // data + crc
} __attribute__((packed)) msp_pkt_t;