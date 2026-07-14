#include "randombytes.h"
#include <stdio.h>
#include <stdlib.h>

int PQCLEAN_randombytes(uint8_t *output, size_t n) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return -1;
    size_t got = fread(output, 1, n, f);
    fclose(f);
    return got == n ? 0 : -1;
}
