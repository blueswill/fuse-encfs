#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"parameter.h"
#include"sm3.h"

#define NEW(type, n) ((type *)malloc(sizeof(type) * (n)))

static int init_flag;

int sm9_init(void) {
    int res;
    if (init_flag)
        return 1;
    res = sm9_parameter_init();
    init_flag = !!res;
    return res;
}

void sm9_release(void) {
    if (!init_flag)
        return;
    sm9_parameter_release();
    init_flag = 0;
}

static int KDF(uint8_t *z, size_t zlen, uint8_t **ha, size_t *halen) {
    int hlen = (int)ceil((5 * logb2(sm9_parameter.param_N)) >> 5) << 3;
    int n = (hlen + (SM3_BIT_SIZE - 1)) >> SM3_BIT_SHIFT;
    size_t size = n << SM3_BYTE_SHIFT;
    size_t left = (((SM3_BIT_SIZE + hlen - 1) & ~SM3_BIT_MASK) - hlen) >> 3;
    uint8_t *res = NEW(uint8_t, size);
    if (!res)
        return -1;
    uint32_t ct = 0x1;
    for (int i = 0; i < n; ++i) {
        z[zlen - 4] = (ct >> 24) & 0xff;
        z[zlen - 3] = (ct >> 16) & 0xff;
        z[zlen - 2] = (ct >> 8) & 0xff;
        z[zlen - 1] = ct & 0xff;
        ++ct;
        if (sm3((const char *)z, zlen, res + i * SM3_BYTE_SIZE) < 0)
            return -1;
    }
    *ha = res;
    *halen = size - left;
    return 0;
}

static int H(uint8_t *z, size_t zlen, big *b) {
    uint8_t *ha;
    size_t len;
    big n1, habig, h;
    if (KDF(z, zlen, &ha, &len) < 0)
        return -1;
    init_big(n1);
    init_big(habig);
    init_big(h);
    decr(sm9_parameter.param_N, 1, n1);
    read_big(habig, ha, len);
    divide(habig, n1, n1);
    incr(habig, 1, h);
    free(ha);
    *b = h;
    return 0;
}

static int H1(const char *id, size_t idlen, int hid, big *b) {
    int ret;
    uint8_t *buf = NEW(uint8_t, idlen + 6);
    if (!buf)
        return -1;
    buf[0] = 0x1;
    buf[idlen + 1] = hid;
    memmove(buf + 1, id, idlen);
    ret = H(buf, idlen + 6, b);
    free(buf);
    return ret;
}

static int H2(const char *id, size_t idlen, int hid, big *b) {
    int ret;
    uint8_t *buf = NEW(uint8_t, idlen + 6);
    if (!buf)
        return -1;
    buf[0] = 0x2;
    buf[idlen + 1] = hid;
    memmove(buf + 1, id, idlen);
    ret = H(buf, idlen + 6, b);
    free(buf);
    return ret;
}
