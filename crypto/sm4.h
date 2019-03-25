#ifndef SM4_H
#define SM4_H

#include<stdint.h>

#define SM4_BLOCK_BIT_SHIFT 7
#define SM4_BLOCK_BYTE_SHIFT (SM4_BLOCK_BIT_SHIFT - 3)
#define SM4_BLOCK_BIT_SIZE (1 << SM4_BLOCK_BIT_SHIFT)
#define SM4_BLOCK_BYTE_SIZE (1 << SM4_BLOCK_BYTE_SHIFT)
#define SM4_BLOCK_BYTE_MASK ((1 << SM4_BLOCK_BYTE_SHIFT) - 1)

#define SM4_KEY_BIT_SHIFT SM4_BLOCK_BIT_SHIFT
#define SM4_KEY_BYTE_SHIFT (SM4_KEY_BIT_SHIFT - 3)
#define SM4_KEY_BIT_SIZE (1 << SM4_KEY_BIT_SHIFT)
#define SM4_KEY_BYTE_SIZE (1 << SM4_KEY_BYTE_SHIFT)

int sm4(const uint32_t *in, const uint32_t *key, uint32_t *out, int encrypt);

/* pass NULL for in to get maximum length for out buffer */
int sm4_cbc(const uint8_t *key, const uint8_t *in, size_t inlen,
        uint8_t *out, size_t *outlen, int encrypt);

#endif
