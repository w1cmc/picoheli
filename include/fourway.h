#ifndef FOURWAY_H
#define FOURWAY_H

enum {
    cmd_InterfaceTestAlive = 0x30,
    cmd_ProtocolGetVersion,
    cmd_InterfaceGetName,
    cmd_InterfaceGetVersion,
    cmd_InterfaceExit,
    cmd_DeviceReset,
    cmd_DeviceGetID, // removed in protocol rev 6/106
    cmd_DeviceInitFlash,
    cmd_DeviceEraseAll,
    cmd_DevicePageErase,
    cmd_DeviceRead,
    cmd_DeviceWrite,
    cmd_DeviceC2CK_LOW,
    cmd_DeviceReadEEprom,
    cmd_DeviceWriteEEprom,
    cmd_InterfaceSetMode,
};

enum {
    ACK_OK,                // 0x00 Operation succeeded. No Error.
    ACK_I_UNKNOWN_ERROR,   // 0x01 Failure in the interface for unknown reason (unused)
    ACK_I_INVALID_CMD,     // 0x02 Interface recognized an unknown command
    ACK_I_INVALID_CRC,     // 0x03 Interface calculated a different CRC / data transmission form Master failed
    ACK_I_VERIFY_ERROR,    // 0x04 Interface did a successful write operation over C2, but the read back data did not match
    ACK_D_INVALID_COMMAND, // 0x05 Device communication failed and the Status was 0x00 instead of 0x0D (unused)
    ACK_D_COMMAND_FAILED,  // 0x06 Device communication failed and the Status was 0x02 or 0x03 instead of 0x0D (unused)
    ACK_D_UNKNOWN_ERROR,   // 0x07 Device communication failed and the Status was of unknow value instead of 0x0D (unused)
    ACK_I_INVALID_CHANNEL, // 0x08 Interface recognized: unavailable ESC Port/Pin is addressed in Multi ESC Mode
    ACK_I_INVALID_PARAM,   // 0x09 Interface recognized an invalid Parameter
    ACK_D_GENERAL_ERROR = 15,   // 0x0F Device communication failed for unknown reason
};

// The 4 ways the interface can be used
enum {
    ifMode_SilC2,  // Silicon Labs C2
    ifMode_SilBLB, // Silicon Labs BLHeli Bootloader
    ifMode_AtmBLB, // Atmel BLHeli Bootloader
    ifMode_AtmSK,  // Atmel SimonK Bootloader
};

typedef struct {
    uint8_t start;
    uint8_t cmd;
    uint8_t addr_msb; // N.B. big-endian
    uint8_t addr_lsb;
    uint8_t param_len;
    uint8_t param[267]; // param + ack + crc
} __attribute__((packed)) pkt_t;

 // 1-255 means n bytes; 0 means 256 bytes.
static inline size_t byte_size(const uint8_t b)
{
    return b ? b : 256;
}

static inline size_t param_len(const pkt_t * pkt)
{
    return byte_size(pkt->param_len);
}

extern void fourway_init();

#endif
