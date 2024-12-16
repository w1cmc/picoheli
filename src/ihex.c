#include <stdio.h>
#include <stdint.h>
#include "ihex.h"

#define MAX_RECORD_LENGTH 16

// Function to write a single Intel HEX record
static void write_record(uint8_t record_length, uint16_t address, uint8_t record_type, const uint8_t *data)
{
    printf(":%02X%04X%02X", record_length, address, record_type);
    uint8_t checksum = record_length + (address >> 8) + (address & 0xFF) + record_type;
    int i;
    for (i = 0; i < record_length; i++) {
        printf("%02X", data[i]);
        checksum += data[i];
    }

    checksum = ~checksum + 1; // Two's complement
    printf("%02X\n", checksum);
}

// Function to convert binary data to Intel HEX format
void ihexify(uint16_t address, const void * data, int size) {
    puts(":020000040000FA"); // Extended Linear Address, 32-bit address with upper 16-bits set to zero, superfluous.

    const uint8_t * ptr = data;
    const uint8_t * const end = &ptr[size];

    while (ptr < end) {
        size_t record_length = end - ptr;
        if (record_length > MAX_RECORD_LENGTH)
            record_length = MAX_RECORD_LENGTH;
        write_record(record_length, address, 0, ptr); // data records
        ptr = &ptr[record_length];
        address += record_length;
    }

    puts(":00000001FF"); // end of file record
}
