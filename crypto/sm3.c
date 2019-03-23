#include<stdint.h>
#include<limits.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include"sm3.h"

#define NEW(type, n) ((type *)malloc(sizeof(type) * (n)))
#define ceil_offset(x) (~(x - 1) & ((1 << 9) - 1))

static inline uint32_t rotate_left(uint32_t i, int n) {
    return (i << n) | (i >> (32 - n));
}
static inline uint16_t reverse16(uint16_t i) {
    return (i >> 8) | (i << 8);
}
static inline uint32_t reverse32(uint32_t i) {
    return reverse16(i >> 16) | (((uint32_t)reverse16(i)) << 16);
}

static inline uint32_t FF(int j, uint32_t x, uint32_t y, uint32_t z) {
    if (j < 16) return x ^ y ^ z;
    return (x & y) | (x & z) | (y & z);
}
static inline uint32_t GG(int j, uint32_t x, uint32_t y, uint32_t z) {
    if (j < 16) return x ^ y ^ z;
    return (x & y) | (~x & z);
}
static inline uint32_t P0(uint32_t x) {
    return x ^ rotate_left(x, 9) ^ rotate_left(x, 17);
}
static inline uint32_t P1(uint32_t x) {
    return x ^ rotate_left(x, 15) ^ rotate_left(x, 23);
}

static uint32_t iv[] = {
    0x7380166f,
    0x4914b2b9,
    0x172442d7,
    0xda8a0600,
    0xa96f30bc,
    0x163138aa,
    0xe38dee4d,
    0xb0fb0e4e
};

uint32_t *extend(const char *str, size_t str_len, size_t *size) {
    uint64_t l = str_len << 3;
    size_t k = ceil_offset(l + 65);
    *size = (l + k + 65) >> 5;
    uint32_t *buf = NEW(uint32_t, *size);
    unsigned char *bc = (unsigned char *)buf;
    if (!buf) return NULL;
    memmove(buf, str, str_len);
    bc[str_len] = 0x80;
    for (unsigned i = 0; i < k / 8; ++i)
        bc[str_len + 1 + i] = 0;
    buf[*size - 2] = l >> 32;
    buf[*size - 1] = l;
    for (unsigned i = 0; i + 2 < *size; ++i) {
        buf[i] = reverse32(buf[i]);
    }
    return buf;
}

static uint32_t *CF(const uint32_t *v, const uint32_t *b) {
    uint32_t *regs = NEW(uint32_t, 8);
    uint32_t *W = NEW(uint32_t, 132);
    if (!regs || !W) {
        free(regs);
        return NULL;
    }
    uint32_t ss1, ss2, tt1, tt2;
    uint32_t *A = regs, *B = regs + 1, *C = regs + 2, *D = regs + 3,
             *E = regs + 4, *F = regs + 5, *G = regs + 6, *H = regs + 7;
    uint32_t Tj;
    int j;
    memmove(regs, v, sizeof(uint32_t) * 8);
    memmove(W, b, sizeof(uint32_t) * 16);
    for (j = 16; j < 68; ++j)
        W[j] = P1(W[j - 16] ^ W[j - 9] ^ rotate_left(W[j - 3], 15)) ^
            rotate_left(W[j - 13], 7) ^ W[j - 6];
    for (j = 68; j < 132; ++j)
        W[j] = W[j - 68] ^ W[j - 68 + 4];
    for (j = 0; j < 64; ++j) {
        if (j < 16) Tj = 0x79cc4519;
        else Tj = 0x7a879d8a;
        ss1 = rotate_left((rotate_left(*A, 12) + *E + rotate_left(Tj, j & 31)), 7);
        ss2 = ss1 ^ rotate_left(*A, 12);
        tt1 = FF(j, *A, *B, *C) + *D + ss2 + W[j + 68];
        tt2 = GG(j, *E, *F, *G) + *H + ss1 + W[j];
        *D = *C;
        *C = rotate_left(*B, 9);
        *B = *A;
        *A = tt1;
        *H = *G;
        *G = rotate_left(*F, 19);
        *F = *E;
        *E = P0(tt2);
    }
    for (j = 0; j < 8; ++j)
        regs[j] ^= v[j];
    free(W);
    return regs;
}

int sm3(const char *str, size_t str_len, uint8_t *outb) {
    size_t len, n, i;
    uint32_t *buf = extend(str, str_len, &len);
    uint32_t (*B)[16] = (void *)buf;
    uint32_t *v = iv;
    if (!buf)
        return -1;
    n = len >> 4;
    for (i = 0; i < n; ++i) {
        uint32_t *vt = CF(v, B[i]);
        if (v != iv)
            free(v);
        if (!vt)
            return -1;
        v = vt;
    }
    for (i = 0; i < 8; ++i) {
        v[i] = reverse32(v[i]);
    }
    memmove(outb, v, sizeof(iv));
    if (v != iv)
        free(v);
    return 0;
}
