/* Hex dump utility
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */
#include <config.h>
#include <stdint.h>
#include <ctype.h>

#include "hexdump.h"

void hexdump(const void *ptr, size_t size, FILE *f_out)
{
    const uint8_t *buffer = (const uint8_t *) ptr;
    unsigned long sum = 0;

    for (size_t n = 0; n < size;) {
        int i;
        enum { BYTES_PER_LINE = 16 };
        char s[BYTES_PER_LINE + 1], hexstring[BYTES_PER_LINE * 3 + 1];

        fprintf(f_out, "%04X  ", (unsigned) n);
        for (i = 0; n < size && i < BYTES_PER_LINE; i++, n++) {
            uint8_t c = buffer[n];
            sum += c;
            sprintf(hexstring + i * 3, "%02X ", c);
            s[i] = isprint(c) ? c : '.';
        }
        s[i] = '\0';
        fprintf(f_out, "%*s\t%s\n", (int) (-3 * BYTES_PER_LINE), hexstring, s);
    }
    fprintf(f_out, "sum = %lu\n", sum);
}
