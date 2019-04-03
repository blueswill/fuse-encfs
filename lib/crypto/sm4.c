#include<stdint.h>
#include<stddef.h>
#include<stdio.h>
#include<string.h>
#include<sys/random.h>
#include"encfs_helper.h"
#include"sm4.h"

static const uint8_t sbox[16][16] = {
    {0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05},
    {0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3, 0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99},
    {0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62},
    {0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6},
    {0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba, 0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8},
    {0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35},
    {0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87},
    {0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52, 0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e},
    {0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1},
    {0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55, 0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3},
    {0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60, 0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f},
    {0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51},
    {0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8},
    {0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd, 0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0},
    {0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84},
    {0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20, 0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48}
};

static const uint32_t FK[] = {
    0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc
};

static const uint32_t CK[] = {
    0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
    0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
    0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
    0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
    0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
    0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
    0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
    0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

static inline uint32_t rotate_left(uint32_t t, int n) {
    return  (t << n) | (t >> (32 - n));
}

static inline uint32_t t(uint32_t i) {
    uint32_t r;
    uint8_t *ib = (void *)&i,
            *ob = (void *)&r;
    for (int s = 0; s < 4; ++s)
        ob[s] = sbox[ib[s] >> 4][ib[s] & 0x0f];
    return r;
}

static inline uint32_t L(uint32_t i) {
    return i ^ rotate_left(i, 2) ^ rotate_left(i, 10) ^
        rotate_left(i, 18) ^ rotate_left(i, 24);
}

static inline uint32_t L1(uint32_t i) {
    return i ^ rotate_left(i, 13) ^ rotate_left(i, 23);
}

static inline uint32_t T(uint32_t i) {
    return L(t(i));
}

static inline uint32_t T1(uint32_t i) {
    return L1(t(i));
}

static inline uint32_t F(uint32_t x0, uint32_t x1, uint32_t x2,
        uint32_t x3, uint32_t rk) {
    return x0 ^ T(x1 ^ x2 ^ x3 ^ rk);
}

static void get_round_key(const uint32_t *key, uint32_t *out) {
    uint32_t k[4];
    for (int i = 0; i < 4; ++i)
        k[i] = key[i] ^ FK[i];
    for (int i = 0; i < 32; i += 4) {
        out[i] = k[0] = k[0] ^ T1(k[1] ^ k[2] ^ k[3] ^ CK[i]);
        out[i + 1] = k[1] = k[1] ^ T1(k[2] ^ k[3] ^ k[0] ^ CK[i + 1]);
        out[i + 2] = k[2] = k[2] ^ T1(k[3] ^ k[0] ^ k[1] ^ CK[i + 2]);
        out[i + 3] = k[3] = k[3] ^ T1(k[0] ^ k[1] ^ k[2] ^ CK[i + 3]);
    }
}

static int sm4_encrypt(const uint32_t *in, const uint32_t *key, uint32_t *out) {
    uint32_t *rk = NEW(uint32_t, 32);
    uint32_t tmp;
    out[0] = in[0], out[1] = in[1], out[2] = in[2], out[3] = in[3];
    if (!rk)
        return -1;
    get_round_key(key, rk);
    for (int i = 4; i <= 32; i += 4) {
        out[0] = F(out[0], out[1], out[2], out[3], rk[i - 4]);
        out[1] = F(out[1], out[2], out[3], out[0], rk[i - 3]);
        out[2] = F(out[2], out[3], out[0], out[1], rk[i - 2]);
        out[3] = F(out[3], out[0], out[1], out[2], rk[i - 1]);
    }
    tmp = out[0];
    out[0] = out[3];
    out[3] = tmp;
    tmp = out[1];
    out[1] = out[2];
    out[2] = tmp;
    free(rk);
    return 0;
}

static int sm4_decrypt(const uint32_t *in, const uint32_t *key, uint32_t *out) {
    uint32_t *rk = NEW(uint32_t, 32);
    uint32_t tmp;
    out[0] = in[0], out[1] = in[1], out[2] = in[2], out[3] = in[3];
    if (!rk)
        return -1;
    get_round_key(key, rk);
    for (int i = 4; i <= 32; i += 4) {
        out[0] = F(out[0], out[1], out[2], out[3], rk[35 - i]);
        out[1] = F(out[1], out[2], out[3], out[0], rk[34 - i]);
        out[2] = F(out[2], out[3], out[0], out[1], rk[33 - i]);
        out[3] = F(out[3], out[0], out[1], out[2], rk[32 - i]);
    }
    tmp = out[0];
    out[0] = out[3];
    out[3] = tmp;
    tmp = out[1];
    out[1] = out[2];
    out[2] = tmp;
    free(rk);
    return 0;
}

int sm4(const uint32_t *in, const uint32_t *key, uint32_t *out, int encrypt) {
    switch (encrypt) {
        case 0:
            return sm4_decrypt(in, key, out);
        default:
            return sm4_encrypt(in, key, out);
    }
}

static int sm4_cbc_encrypt(const uint8_t *key, const uint8_t *in, size_t inlen,
        uint8_t *out, size_t *outlen) {
    size_t max = SM4_BLOCK_BYTE_SIZE + (inlen | SM4_BLOCK_BYTE_MASK) + 1;
    int ret = -1;
    uint8_t padding = ~(inlen | ~SM4_BLOCK_BYTE_MASK) + 1;
    uint8_t *key32, *data, *outbuf;
    size_t i, j;
    *outlen = max;
    if (!in) {
        return 0;
    }
    /* for correct alignment */
    key32 = NEW(uint32_t, SM4_KEY_BYTE_SIZE >> 2);
    data = NEW(uint32_t, (max - SM4_BLOCK_BYTE_SIZE) >> 2);
    outbuf = NEWZ(uint32_t, max >> 2);
    if (!key32 || !data || !outbuf)
        goto end;
    memmove(key32, key, SM4_KEY_BYTE_SIZE);
    memmove(data, in, inlen);
#ifndef CHECK
    getrandom(outbuf, SM4_BLOCK_BYTE_SIZE, 0);
#endif
    for (i = 0; i < padding; ++i)
        data[inlen + i] = padding;
    for (i = 0; i + SM4_BLOCK_BYTE_SIZE < max; i += SM4_BLOCK_BYTE_SIZE) {
        for (j = 0; j < SM4_BLOCK_BYTE_SIZE; ++j)
            data[i + j] ^= outbuf[i + j];
        if (sm4_encrypt((const uint32_t *)(data + i), (const uint32_t *)key32,
                    (uint32_t *)(outbuf + i + SM4_BLOCK_BYTE_SIZE)) < 0)
            goto end;
    }
    memmove(out, outbuf, max);
    ret = 0;
end:
    free(key32);
    free(data);
    free(outbuf);
    return ret;
}

static int sm4_cbc_decrypt(const uint8_t *key, const uint8_t *in, size_t inlen,
        uint8_t *out, size_t *outlen) {
    int ret = -1;
    uint8_t *key32, *data, *outbuf;
    size_t i, j, k, padding;
    if (inlen < SM4_BLOCK_BYTE_SIZE || (inlen & SM4_BLOCK_BYTE_MASK))
        return -1;
    if (!in) {
        *outlen = inlen - SM4_BLOCK_BYTE_SIZE;
        return 0;
    }
    key32 = NEW(uint32_t, SM4_KEY_BYTE_SIZE >> 2);
    data = NEW(uint32_t, inlen >> 2);
    outbuf = NEW(uint32_t, (inlen - SM4_BLOCK_BYTE_SIZE) >> 2);
    if (!key32 || !data || !outbuf)
        goto end;
    memmove(key32, key, SM4_KEY_BYTE_SIZE);
    memmove(data, in, inlen);
    for (i = SM4_BLOCK_BYTE_SIZE, j = 0; i < inlen;
            j = i, i += SM4_BLOCK_BYTE_SIZE) {
        if (sm4_decrypt((const uint32_t *)(data + i),
                    (const uint32_t *)key32, (uint32_t *)(outbuf + j)) < 0)
            goto end;
        for (k = 0; k < SM4_BLOCK_BYTE_SIZE; ++k)
            outbuf[j + k] ^= data[j + k];
    }
    padding = outbuf[inlen - SM4_BLOCK_BYTE_SIZE - 1];
    if (inlen <= (size_t)SM4_BLOCK_BYTE_SIZE + padding)
        goto end;
    *outlen = inlen - SM4_BLOCK_BYTE_SIZE - padding;
    memmove(out, outbuf, *outlen);
    ret = 0;
end:
    free(key32);
    free(data);
    free(outbuf);
    return ret;
}

int sm4_cbc(const uint8_t *key, const uint8_t *in, size_t inlen,
        uint8_t *out, size_t *outlen,
        int encrypt) {
    switch (encrypt) {
        case 0:
            return sm4_cbc_decrypt(key, in, inlen, out, outlen);
        default:
            return sm4_cbc_encrypt(key, in, inlen, out, outlen);
    }
}
