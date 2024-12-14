#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "putbuf.h"

void putbuf(const uint8_t * const buf, size_t size)
{
    if (size == 0) {
        puts("no data received");
        return;
    }

    int i;
    for (i=0; i<size; ++i) {
        printf("%02x", buf[i]);
        putchar((i & 7) == 7 ? '\n' : ' ');
    }
    
    if (i & 7)
        putchar('\n');
    
    for(i=0; i<size; ++i) {
        if (isprint(buf[i]))
            putchar(buf[i]);
        else
            putchar('.');
        if ((i & 31) == 31)
            putchar('\n');
    }

    if (i & 15)
        putchar('\n');
}
