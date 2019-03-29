#ifndef ENCFS_HELPER_H
#define ENCFS_HELPER_H

#include<stdlib.h>

#define NEW(type, n) (malloc(sizeof(type) * (n)))
#define NEWZ(type, n) (calloc(n, sizeof(type)))
#define NEW1(type) NEW(type, 1)
#define NEWZ1(type) NEWZ(type, 1)

#define BIT_MASK(n) ((1 << n) - 1)
#define CEIL_OFFSET2(x, n) (~((x) - 1) & BIT_MASK(n))
#define FLOOR_OFFSET2(x, n) ((x) & BIT_MASK(n))
#define CEIL2(x, n) ((((x) - 1) | BIT_MASK(n)) + 1)
#define FLOOR2(x, n) ((x) & ~BIT_MASK(n))
#define INDEX2(x, n) ((x) >> (n))
#define SIZE2(n) (1 << (n))

#endif
