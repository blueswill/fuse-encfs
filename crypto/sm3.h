#ifndef SM3_H
#define SM3_H

#include<stdint.h>

#define SM3_BIT_SHIFT   8
#define SM3_BYTE_SHIFT  5
#define SM3_BIT_SIZE (1 << SM3_BIT_SHIFT)
#define SM3_BIT_MASK (SM3_BIT_SIZE - 1)
#define SM3_BYTE_SIZE (1 << SM3_BYTE_SHIFT)

int sm3(const char *str, size_t str_len, uint8_t *outb);

#endif
