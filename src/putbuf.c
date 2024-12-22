#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "putbuf.h"

void ascbuf(int c, int eol)
{
    static char buf[18] = {0};
    static char * const end = &buf[sizeof(buf) - 1];
    static char * pos = buf;

    printf("%02X ", c);
    *pos++ = isprint(c) ? c : '.';
    if (eol || (pos == end)) {
        *pos = 0;
        puts(buf);
        pos = buf;
    }
}

void putbuf(const uint8_t * buf, size_t size)
{
    if (size == 0) {
        puts("no data received");
        return;
    }

    const uint8_t * const end = &buf[size];
    while (buf < end)
        ascbuf(*buf, ++buf == end);
}
