#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pico/unique_id.h>
#include "usb_tx.h"
#include "msp_protocol.h"
#include "msp.h"

#define FC_VERSION_LENGTH 3
#define FC_VERSION_MAJOR 0
#define FC_VERSION_MINOR 0
#define FC_VERSION_PATCH 1

static const char build_info[] = BUILD_INFO;
static const size_t build_info_len = sizeof(build_info) - 1; // omit trailing nul

static const char *cmd_label(const msp_pkt_t *const pkt)
{
    switch (pkt->cmd) {
    case MSP_API_VERSION:
        return "MSP_API_VERSION";
    case MSP_FC_VARIANT:
        return "MSP_FC_VARIANT";
    case MSP_FC_VERSION:
        return "MSP_FC_VERSION";
    case MSP_BOARD_INFO:
        return "MSP_BOARD_INFO";
    case MSP_BUILD_INFO:
        return "MSP_BUILD_INFO";
    case MSP_UID:
        return "MSP_UID";
    case MSP_MOTOR:
        return "MSP_MOTOR";
    case MSP_FEATURE_CONFIG:
        return "MSP_FEATURE_CONFIG";
    case MSP_BATTERY_STATE:
        return "MSP_BATTERY_STATE";
    case MSP_SET_PASSTHROUGH:
        return "MSP_SET_PASSTHROUGH";
    default:
        break;
    }

    return NULL;
}

void msp_handle_pkt(msp_pkt_t *pkt)
{
    const char * const label = cmd_label(pkt);
    printf("%s (%u) size=%d\n", label ? label : "unrecognized command", pkt->cmd, pkt->size);

    switch (pkt->cmd) {
        case MSP_API_VERSION:
            pkt->size = API_VERSION_LENGTH + 1;
            pkt->data[0] = MSP_PROTOCOL_VERSION;
            pkt->data[1] = API_VERSION_MAJOR;
            pkt->data[2] = API_VERSION_MINOR;
            break;
        case MSP_FC_VARIANT:
            pkt->size = 4;
            memcpy(pkt->data, "RASP", 4);
            break;
        case MSP_FC_VERSION:
            pkt->size = FC_VERSION_LENGTH;
            pkt->data[0] = FC_VERSION_MAJOR;
            pkt->data[1] = FC_VERSION_MINOR;
            pkt->data[2] = FC_VERSION_PATCH;
            break;
        case MSP_BUILD_INFO:
            pkt->size = build_info_len;
            memcpy(pkt->data, build_info, build_info_len);
            break;
        case MSP_BOARD_INFO:
            pkt->size = 6;
            memcpy(pkt->data, "PICO", 4);
            pkt->data[4] = 1; // little-endian 16-bit version number
            pkt->data[5] = 0;
            break;
        case MSP_UID:
            pkt->size = 12;
            pico_get_unique_board_id((pico_unique_board_id_t *) pkt->data); // only fills first 8 bytes
            bzero(&pkt->data[8], 4);
            break;
        case MSP_MOTOR:
            pkt->size = 2; // 2 bytes per motor, but just one motor at rest (PWM min throttle 1000 us)
            pkt->data[0] = (1000 & 255); // little endian.
            pkt->data[1] = (1000 >> 8);
            break;
        case MSP_FEATURE_CONFIG:
            pkt->size = 4;
            bzero(pkt->data, 4); // no features
            break;
        case MSP_BATTERY_STATE:
            // Fictional battery: 4S LiPo, 3,300 mAh, 4.2 V per cell
            pkt->size = 11;
            pkt->data[0] = 4;
            pkt->data[1] = (3300 & 255);
            pkt->data[2] = (3300 >> 8);
            pkt->data[3] = 42; // 4.2 volts times ten
            pkt->data[4] = (100 & 255);
            pkt->data[5] = (100 >> 8);
            pkt->data[6] = 1;
            pkt->data[7] = 0;
            pkt->data[8] = 1; // battery "state". Don't know the encoding
            pkt->data[9] = (420 & 255); // 4.2 volts again, this time times 100
            pkt->data[10] = (420 >> 8);
            break;
        case MSP_SET_PASSTHROUGH:
            pkt->size = 1;
            pkt->data[0] = 1; // return the number of connected ESCs
            break;
        default:
            return;
    }

    pkt->direction = '>';
    // CRC is XOR of command, size and any data: not a very good CRC.
    uint8_t crc = pkt->cmd ^ pkt->size;
    uint8_t * ptr = pkt->data;
    uint8_t * end = &ptr[pkt->size];
    while (ptr < end)
        crc ^= *ptr++;
    *end++ = crc;
    usb_tx_buf(pkt, end - pkt->preamble);
}
