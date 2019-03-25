#ifndef ENCFS_HELPER_H
#define ENCFS_HELPER_H

#include<stdlib.h>

#define NEW(type, n) (malloc(sizeof(type) * (n)))
#define NEWZ(type, n) (calloc(n, sizeof(type)))
#define NEW1(type) NEW(type, 1)
#define NEWZ1(type) NEWZ(type, 1)

#define CEIL_OFFSET2(x, n) (~((x) - 1) & ((1 << (n)) - 1))

#endif
