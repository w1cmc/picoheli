#include <stdio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <queue.h>
#include <tusb.h>
#include "onewire.h"
#include "blheli.h"
#include "fourway.h"

#define FOURWAY_TASK_STACK_SIZE 1024
#define INTERFACE_VERSION 0xC800U
#define IF_VER_HI ((INTERFACE_VERSION) >> 8)
#define IF_VER_LO ((INTERFACE_VERSION) & 255)

#define PROTO_VER 106

static const UBaseType_t pktQueueLength = 8;
static QueueHandle_t pktQueueHandle;
static TaskHandle_t fourwayTaskHandle;
static SemaphoreHandle_t txAvailMutex;

static uint16_t crc16_xmodem(const uint8_t data, uint16_t crc)
{
    static const uint16_t lut[] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823, 
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067, 
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
    };

	return (crc << 8) ^ lut[(crc >> 8) ^ data];
}

static uint16_t crc16_range(const uint8_t * ptr, const uint8_t * const end)
{
    uint16_t crc = 0;
    while (ptr < end)
        crc = crc16_xmodem(*ptr++, crc);
    return crc;
}

static pkt_t *fsm(int c)
{
    typedef enum { IDLE, START, COMMAND, ADDRESS_HI, ADDRESS_LO, PARAM_LEN, PARAM, CRC_HI, CRC_LO } state_t;
    static state_t curr = IDLE;
    static uint8_t param_cnt;
    static pkt_t pkt = {0};
    state_t next = IDLE;

    switch (curr) {
        case IDLE:
            if (c == 0x2F)
                next = START;
            break;
        case START:
            if (0x30 <= c && c <= 0x3F)
                next = COMMAND;
            break;
        case COMMAND:
        case ADDRESS_HI:
        case ADDRESS_LO:
        case PARAM_LEN:
        case CRC_HI:
            next = curr + 1;
            break;
        case PARAM:
            if (param_cnt == pkt.param_len)
                next = CRC_HI;
            else
                next = PARAM;
            break;
        case CRC_LO:
            if (c == 0x2F)
                next = START;
            break;
    }

    curr = next;
    
    switch (curr) {
        case START:
            pkt.start = c;
            break;
        case COMMAND:
            pkt.cmd = c;
            break;
        case ADDRESS_HI:
            pkt.addr_msb = c;
            break;
        case ADDRESS_LO:
            pkt.addr_lsb = c;
            break;
        case PARAM_LEN:
            pkt.param_len = c;
            param_cnt = 0;
            break;
        case PARAM:
            pkt.param[param_cnt++] = c;
            break;
        case CRC_HI:
            pkt.param[param_cnt++] = c;
            break;
        case CRC_LO:
            pkt.param[param_cnt++] = c;
            return &pkt;
        default:
            break;
    }

    return NULL;
}

static const char * cmd_label(int cmd)
{
    static const char *labels[] = {
        "Interface Test Alive",
        "Protocol Get Version",
        "Interface Get Name",
        "Interface Get Version",
        "Interface Exit",
        "Device Reset",
        "Device Get ID (deprecated)",
        "Device Init Flash",
        "Device Erase All",
        "Device Page Erase",
        "Device Read",
        "Device Write",
        "Device C2CK LOW",
        "Device Read EEPROM",
        "Device Write EEPROM",
        "Interface Set Mode",
    };

    return labels[cmd & 15];
}

static bool check_crc(const pkt_t * pkt)
{
    return !crc16_range(&pkt->start, &pkt->param[pkt->param_len + sizeof(uint16_t)]);
}

static void handle_pkt(pkt_t * pkt)
{
    const bool crc_ok = check_crc(pkt);
    printf("Cmd=%02X (%s) Addr=%04X Param_len=%d Param=%d CRC %s\n",
        pkt->cmd,
        cmd_label(pkt->cmd),
        pkt->addr_lsb | (pkt->addr_msb << 8),
        pkt->param_len,
        pkt->param[0],
        crc_ok ? "OK" : "bad");

    pkt->start ^= 1; // flip the LSB to indicate a reply

    uint8_t ack = crc_ok ? ACK_OK : ACK_I_INVALID_CRC;

    switch (pkt->cmd) {
    case cmd_InterfaceTestAlive:
        pkt->param_len = 1;
        pkt->param[0] = 0;
        break;
    case cmd_ProtocolGetVersion:
        pkt->param_len = 1;
        pkt->param[0] = PROTO_VER;
        break;
    case cmd_InterfaceGetName:
        pkt->param_len = 8;
        memcpy(pkt->param, "Pico4way", pkt->param_len);
        break;
    case cmd_InterfaceGetVersion:
        pkt->param_len = 2;
        pkt->param[0] = IF_VER_HI; // big-endian
        pkt->param[1] = IF_VER_LO;
        break;
    case cmd_InterfaceExit:
        pkt->param_len = 1;
        pkt->param[0] = 0;
        break;
    case cmd_DeviceReset:
        ack = blheli_DeviceReset(pkt);
        break;
    case cmd_DeviceInitFlash:
        ack = blheli_DeviceInitFlash(pkt);
        break;
    case cmd_DeviceEraseAll:
        ack = ACK_I_INVALID_CMD; // not valid for SilBLB nor AtmBLB
        break;
    case cmd_InterfaceSetMode:
        if (pkt->param_len != 1 || pkt->param[0] != 1)
            ack = ACK_I_INVALID_PARAM;
        break;
    case cmd_DeviceRead:
        ack = blheli_DeviceRead(pkt);
        break;
    default:
        return;
    }

    uint8_t * ptr = &pkt->param[pkt->param_len];
    *ptr++ = ack;
    const uint16_t crc = crc16_range(&pkt->start, ptr);
    *ptr++ = (crc >> 8); // big-endian
    *ptr++ = (crc & 255);

    uint8_t * pos = &pkt->start;
    while (pos < ptr) {
        uint32_t tx_size = tud_cdc_write_available();
        if (tx_size > ptr - pos)
            tx_size = ptr - pos;
        if (tx_size > 0)
            tud_cdc_write(pos, tx_size);
        else
            xSemaphoreTake(txAvailMutex, pdMS_TO_TICKS(50));
        pos += tx_size;
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    int c;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (tud_cdc_available() > 0 && (c = tud_cdc_read_char()) >= 0) {
        pkt_t * const pkt = fsm(c);
        if (pkt)
            xQueueSendFromISR(pktQueueHandle, pkt, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    xSemaphoreGive(txAvailMutex);
}

static void fourway_task_func(void *param)
{
    while (1) {
        pkt_t pkt;
        if (xQueueReceive(pktQueueHandle, &pkt, portMAX_DELAY) == pdPASS)
            handle_pkt(&pkt);
    }
}

void fourway_init()
{
    pktQueueHandle = xQueueCreate(pktQueueLength, sizeof(pkt_t));
    txAvailMutex = xSemaphoreCreateBinary();
    configASSERT(pktQueueHandle);
    configASSERT(xTaskCreate(fourway_task_func, "4w-if", FOURWAY_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &fourwayTaskHandle));
    configASSERT(fourwayTaskHandle);
}
