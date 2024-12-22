
#include <tusb.h>
#include "fourway.h"
#include "putbuf.h"
#include "usb_rx.h"

static fourway_pkt_t *fsm(int c)
{
    typedef enum {
        IDLE,
        FOURWAY_START,
        FOURWAY_COMMAND,
        FOURWAY_ADDRESS_HI,
        FOURWAY_ADDRESS_LO,
        FOURWAY_PARAM_LEN,
        FOURWAY_PARAM,
        FOURWAY_CRC_HI,
        FOURWAY_CRC_LO,
        MSP_DOLLAR,
        MSP_M,
        MSP_LESS,
        MSP_SIZE,
        MSP_COMMAND,
        MSP_DATA,
        MSP_CRC,
    } state_t;
    static state_t curr = IDLE;
    static size_t param_cnt;
    static fourway_pkt_t pkt = {0};
    state_t next = IDLE;

    switch (curr) {
        case IDLE:
            if (c == 0x2f)
                next = FOURWAY_START;
            break;
        case FOURWAY_START:
            if (0x30 <= c && c <= 0x3F)
                next = FOURWAY_COMMAND;
            break;
        case FOURWAY_COMMAND:
        case FOURWAY_ADDRESS_HI:
        case FOURWAY_ADDRESS_LO:
        case FOURWAY_PARAM_LEN:
        case FOURWAY_CRC_HI:
            next = curr + 1;
            break;
        case FOURWAY_PARAM:
            if (param_cnt == fourway_param_len(&pkt))
                next = FOURWAY_CRC_HI;
            else
                next = FOURWAY_PARAM;
            break;
        case FOURWAY_CRC_LO:
            if (c == 0x2F)
                next = FOURWAY_START;
            break;
    }

    curr = next;
    
    switch (curr) {
        case FOURWAY_START:
            bzero(&pkt, sizeof(pkt));
            pkt.start = c;
            break;
        case FOURWAY_COMMAND:
            pkt.cmd = c;
            break;
        case FOURWAY_ADDRESS_HI:
            pkt.addr_msb = c;
            break;
        case FOURWAY_ADDRESS_LO:
            pkt.addr_lsb = c;
            break;
        case FOURWAY_PARAM_LEN:
            pkt.param_len = c;
            param_cnt = 0;
            break;
        case FOURWAY_PARAM:
            pkt.param[param_cnt++] = c;
            break;
        case FOURWAY_CRC_HI:
            pkt.param[param_cnt++] = c;
            break;
        case FOURWAY_CRC_LO:
            pkt.param[param_cnt++] = c;
            return &pkt;
        default:
            break;
    }

    return NULL;
}

void tud_cdc_rx_cb(uint8_t itf)
{
    int c;

    while (tud_cdc_available() > 0 && (c = tud_cdc_read_char()) >= 0) {    
        fourway_pkt_t * const pkt = fsm(c);
        ascbuf(c, pkt != NULL);
        if (pkt)
            xQueueSendToBack(fourwayQueueHandle, pkt, 0);
    }
}
