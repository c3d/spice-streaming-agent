#undef NDEBUG
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "hexdump.h"

static char buffer[64 * 1024];

int main(int argc, const char **argv)
{
    assert(argc >= 2);
    size_t s = fread(buffer, 1, sizeof(buffer), stdin);
    assert(feof(stdin) && !ferror(stdin) && s <= sizeof(buffer));
    FILE *f = fopen(argv[1], "w");
    assert(f);
    hexdump(buffer, s, f);
    fclose(f);
    return 0;
}
