
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <tusb.h>
#include "fourway.h"
#include "msp.h"
#include "usb_rx.h"

#define USB_RX_TASK_STACK_SIZE 1024

static const UBaseType_t pktQueueLength = 8;
static QueueSetHandle_t xQueueSet;
static QueueHandle_t fourwayQueueHandle;
static QueueHandle_t mspQueueHandle;
static TaskHandle_t usbRxTaskHandle;

static int fsm(int c)
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
    static union {
        fourway_pkt_t fourway;
        msp_pkt_t msp;
    } pkt;
    state_t next = IDLE;

    switch (curr) {
        case IDLE:
            if (c == '/')
                next = FOURWAY_START;
            else if (c == '$')
                next = MSP_DOLLAR;
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
        case MSP_SIZE:
        case MSP_LESS:
            next = curr + 1;
            break;
        case FOURWAY_PARAM:
            if (param_cnt == fourway_param_len(&pkt.fourway))
                next = FOURWAY_CRC_HI;
            else
                next = FOURWAY_PARAM;
            break;
        case FOURWAY_CRC_LO:
        case MSP_CRC:
            if (c == '/')
                next = FOURWAY_START;
            else if (c == '$')
                next = MSP_DOLLAR;
            break;
        case MSP_DOLLAR:
            if (c == 'M')
                next = MSP_M;
            break;
        case MSP_M:
            if (c == '<')
                next = MSP_LESS;
            break;
        case MSP_COMMAND:
            if (param_cnt)
                next = MSP_DATA;
            else
                next = MSP_CRC;
            break;
        case MSP_DATA:
            if (param_cnt == pkt.msp.size)
                next = MSP_CRC;
            break;
        default:
            break;
    }

    curr = next;
    
    switch (curr) {
        case FOURWAY_START:
            bzero(&pkt, sizeof(pkt));
            pkt.fourway.start = c;
            break;
        case FOURWAY_COMMAND:
            pkt.fourway.cmd = c;
            break;
        case FOURWAY_ADDRESS_HI:
            pkt.fourway.addr_msb = c;
            break;
        case FOURWAY_ADDRESS_LO:
            pkt.fourway.addr_lsb = c;
            break;
        case FOURWAY_PARAM_LEN:
            pkt.fourway.param_len = c;
            param_cnt = 0;
            break;
        case FOURWAY_PARAM:
            pkt.fourway.param[param_cnt++] = c;
            break;
        case FOURWAY_CRC_HI:
            pkt.fourway.param[param_cnt++] = c;
            break;
        case FOURWAY_CRC_LO:
            pkt.fourway.param[param_cnt++] = c;
            xQueueSendToBack(fourwayQueueHandle, &pkt.fourway, 0);
            return 1;
        case MSP_DOLLAR:
            bzero(&pkt, sizeof(pkt));
            pkt.msp.preamble[0] = c;
            break;
        case MSP_M:
            pkt.msp.preamble[1] = c;
            break;
        case MSP_LESS:
            pkt.msp.direction = c;
            break;
        case MSP_SIZE:
            pkt.msp.size = c;
            param_cnt = 0;
            break;
        case MSP_COMMAND:
            pkt.msp.cmd = c;
            break;
        case MSP_DATA:
        case MSP_CRC:
            pkt.msp.data[param_cnt++] = c;
            xQueueSendToBack(mspQueueHandle, &pkt.msp, 0);
            break;
        default:
            break;
    }

    return 0;
}

void tud_cdc_rx_cb(uint8_t itf)
{
    int c;

    while (tud_cdc_available() > 0 && (c = tud_cdc_read_char()) >= 0) {
        fsm(c);
    }
}

static void usb_rx_task_func(void *param)
{
    while (1) {
        QueueSetMemberHandle_t xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
        if (xActivatedMember == fourwayQueueHandle) {
            fourway_pkt_t pkt;
            if (xQueueReceive(fourwayQueueHandle, &pkt, 0) == pdPASS)
                fourway_handle_pkt(&pkt);
        }
        else if (xActivatedMember == mspQueueHandle) {
            msp_pkt_t pkt;
            if (xQueueReceive(mspQueueHandle, &pkt, 0) == pdPASS)
                msp_handle_pkt(&pkt);
        }
    }
}

void usb_rx_init()
{
    xQueueSet = xQueueCreateSet(2 * pktQueueLength);
    fourwayQueueHandle = xQueueCreate(pktQueueLength, sizeof(fourway_pkt_t));
    configASSERT(fourwayQueueHandle);
    xQueueAddToSet(fourwayQueueHandle, xQueueSet);
    mspQueueHandle = xQueueCreate(pktQueueLength, sizeof(msp_pkt_t));
    configASSERT(mspQueueHandle);
    xQueueAddToSet(mspQueueHandle, xQueueSet);
    configASSERT(xTaskCreate(usb_rx_task_func, "USB rx", USB_RX_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &usbRxTaskHandle));
    configASSERT(usbRxTaskHandle);
}
