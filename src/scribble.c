#include "scribble.h"

void scribble(void * buf, int size)
{
    static const char fill[] = "Hey Rocky! Watch me pull a rabbit out of my hat.";
    const char * src = fill;
    const char * const src_end = &src[sizeof(fill) - 1]; // omit nul terminator
    char * dst = (char *) buf;
    char * const dst_end = &dst[size];
    while (dst < dst_end) {
        *dst++ = *src++;
        if (src == src_end)
            src = fill;
    }
    dst_end[-1] = 4; // ASCII EOT (end of transmission)
}
