#include<string.h>
#include<gmodule.h>
#include"sm4.h"

static int shift_left(uint32_t *b) {
    int r = 0;
    for (int32_t i = 3; i >= 0; --i) {
        int32_t tmp = b[i];
        b[i] = (b[i] << 1) | r;
        r = (tmp < 0);
    }
    return r;
}

static void GF_mult(uint32_t *b, int r) {
    for (int i = 0; i < r; ++i) {
        int s = shift_left(b);
        b[3] ^= s * 135;
    }
}

static int sm4_xts_one(const uint32_t *key,
        const uint32_t *data, uint32_t *out, const uint32_t *iv, int encrypt) {
    uint32_t outbuf[4];
    for (size_t i = 0; i < 4; ++i)
        outbuf[i] = data[i] ^ iv[i];
    int ret = sm4(outbuf, key, out, encrypt);
    if (ret < 0)
        return ret;
    for (size_t i = 0; i < 4; ++i)
        out[i] ^= iv[i];
    return ret;
}

int sm4_xts(const uint8_t *key, const uint8_t *data, size_t datalen,
        uint8_t *out, const uint8_t *tweak, int j, int encrypt) {
    uint32_t key32[SM4_XTS_KEY_BYTE_SIZE >> 2];
    uint32_t iv[SM4_XTS_IV_BYTE_SIZE >> 2];
    uint32_t ivcipher[SM4_XTS_IV_BYTE_SIZE >> 2];
    uint32_t *in = NULL, *outbuf = NULL;
    uint32_t *key1 = key32, *key2 = key32 + 4;
    uint32_t *ptr, *end, *o;
    int ret = -1;
    if (datalen & SM4_BLOCK_BYTE_MASK)
        return -1;
    if (!(in = g_new(uint32_t, datalen >> 2)) ||
            !(outbuf = g_new(uint32_t, datalen >> 2)))
        goto end;
    memmove(key32, key, SM4_XTS_KEY_BYTE_SIZE);
    memmove(in, data, datalen);
    memmove(iv, tweak, SM4_XTS_IV_BYTE_SIZE);
    if (sm4(iv, key2, ivcipher, 1) < 0)
        goto end;
    ptr = in; end = ptr + (datalen >> 2);
    o = outbuf;
    GF_mult(ivcipher, j);
    while (ptr != end) {
        if (sm4_xts_one(key1, ptr, o, ivcipher, encrypt) < 0)
            goto end;
        ptr += (SM4_BLOCK_BYTE_SIZE >> 2);
        o += (SM4_BLOCK_BYTE_SIZE >> 2);
        GF_mult(ivcipher, 1);
    }
    memmove(out, outbuf, datalen);
    ret = 0;
end:
    g_free(in);
    g_free(outbuf);
    return ret;
}
