#include<math.h>
#include<string.h>
#include"sm9_helper.h"
#include"parameter.h"
#include"sm3.h"

int KDF(struct _string *z, size_t hlen, struct _string *ret) {
    int n = (hlen + (SM3_BIT_SIZE - 1)) >> SM3_BIT_SHIFT;
    size_t size = n << SM3_BYTE_SHIFT;
    size_t left = (((SM3_BIT_SIZE + hlen - 1) & ~SM3_BIT_MASK) - hlen) >> 3;
    uint8_t *res = NEW(uint8_t, size);
    if (!res)
        return -1;
    uint32_t ct = 0x1;
    for (int i = 0; i < n; ++i) {
        z->buf[z->size - 4] = (ct >> 24) & 0xff;
        z->buf[z->size - 3] = (ct >> 16) & 0xff;
        z->buf[z->size - 2] = (ct >> 8) & 0xff;
        z->buf[z->size - 1] = ct & 0xff;
        ++ct;
        if (sm3((const char *)z->buf, z->size, res + i * SM3_BYTE_SIZE) < 0)
            return -1;
    }
    ret->buf = res;
    ret->size = size - left;
    return 0;
}

static int H(struct _string *z, big *b) {
    struct _string ha;
    int hlen = (int)ceil((5 * logb2(sm9_parameter.param_N)) >> 5) << 3;
    big n1, habig, h;
    if (KDF(z, hlen, &ha) < 0)
        return -1;
    init_big(n1);
    init_big(habig);
    init_big(h);
    decr(sm9_parameter.param_N, 1, n1);
    read_big(habig, ha.buf, ha.size);
    divide(habig, n1, n1);
    incr(habig, 1, h);
    free(ha.buf);
    *b = h;
    release_big(n1);
    release_big(habig);
    return 0;
}

int H1(const struct _string *id, uint8_t hid, big *b) {
    struct _string str;
    int ret;
    str.size = 2 + id->size + KDF_EXTRA_BYTE_LEN;
    str.buf = NEW(uint8_t, str.size);
    if (!str.buf)
        return -1;
    str.buf[0] = 0x01;
    str.buf[id->size + 1] = hid;
    memmove(str.buf + 1, id->buf, id->size);
    ret = H(&str, b);
    free(str.buf);
    return ret;
}

int H2(const struct _string *id, uint8_t hid, big *b) {
    struct _string str;
    int ret;
    str.size = 2 + id->size + KDF_EXTRA_BYTE_LEN;
    str.buf = NEW(uint8_t, str.size);
    if (!str.buf)
        return -1;
    str.buf[0] = 0x02;
    str.buf[id->size + 1] = hid;
    memmove(str.buf + 1, id->buf, id->size);
    ret = H(&str, b);
    free(str.buf);
    return ret;
}

int write_big(big r, struct _string *s) {
    size_t len = big_size(r);
    uint8_t *buf = NEW(uint8_t, len);
    if (buf) {
        len = big_to_bytes(len, r, (void *)buf, 0);
        s->buf = buf;
        s->size = len;
        return 0;
    }
    return -1;
}

int write_epoint(epoint *e, struct _string *s) {
    big x, y;
    size_t size = epoint_size(e, &x, &y);
    uint8_t *buf = NEW(uint8_t, size);
    size_t r;
    r = write_big_buf(x, buf, size);
    r += write_big_buf(y, buf + r, size - r);
    s->buf = buf;
    s->size = r;
    return 0;
}

size_t write_epoint_buf(epoint *e, uint8_t *buf, size_t size) {
    big x, y;
    size_t s;
    epoint_size(e, &x, &y);
    s = write_big_buf(x, buf, size);
    s += write_big_buf(y, buf + s, size - s);
    release_big(x);
    release_big(y);
    return s;
}

size_t epoint_size(epoint *e, big *xr, big *yr) {
    size_t ret;
    big x, y;
    init_big(x);
    init_big(y);
    epoint_get(e, x, y);
    ret = big_size(x) + big_size(y);
    if (xr) *xr = x;
    else release_big(x);
    if (yr) *yr = y;
    else release_big(y);
    return ret;
}

int read_epoint(epoint *e, const uint8_t *b, size_t blen) {
    big x, y;
    if (blen < 2 * BNLEN)
        return -1;
    init_big(x);
    init_big(y);
    read_big(x, b, BNLEN);
    read_big(y, b + BNLEN, BNLEN);
    epoint_set(x, y, 0, e);
    release_big(x);
    release_big(y);
    return 0;
}

int read_ecn2_byte128(ecn2 *v, const unsigned char *buf) {
    zzn2 x, y;
    big a=NULL, b=NULL;

    init_zzn2(x);
    init_zzn2(y);
    init_big(a);
    init_big(b);

    read_big(b, buf, BNLEN);
    read_big(a, buf + BNLEN, BNLEN);
    zzn2_from_bigs(a, b, &x);
    read_big(b, buf + BNLEN * 2, BNLEN);
    read_big(a, buf + BNLEN * 3, BNLEN);
    zzn2_from_bigs(a, b, &y);
    int ret = ecn2_set(&x, &y, v);

    release_zzn2(x);
    release_zzn2(y);
    release_big(a);
    release_big(b);
    return ret;
}

static size_t big_write_ecn2(big t, char *b, size_t blen) {
    big r;
    init_big(r);
    redc(t, r);
    size_t ret = write_big_buf(r, b, blen);
    release_big(r);
    return ret;
}

size_t write_ecn2(ecn2 *v, char *b, size_t blen) {
    size_t r;
    r = big_write_ecn2(v->x.b, b, blen);
    r += big_write_ecn2(v->x.a, b + r, blen - r);
    r += big_write_ecn2(v->y.b, b + r, blen - r);
    r += big_write_ecn2(v->y.a, b + r, blen - r);
    return r;
}
