#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include"sm9.h"
#include"parameter.h"
#include"sm9_helper.h"
#include"zzn12/zzn12_wrapper.h"
#include"sm4.h"
#include"sm3.h"

void report_error(enum sm9_error e) {}

void master_key_pair_free(struct master_key_pair *pair) {
    release_big(pair->priv);
    release_epoint(pair->pub);
    free(pair);
}

void private_key_free(struct private_key *key) {
    release_ecn2(key->e);
    free(key);
}

static int is_zero(const struct _string *s) {
    for (size_t i = 0; i < s->size; ++i)
        if (s->buf[i])
            return 0;
    return 1;
}

static int mac(const struct _string *key, const struct _string *z, struct _string *out) {
    int ret = 0;
    struct _string tmp;
    tmp.buf = NULL;
    if (!NEW_STRING(&tmp, key->size + z->size))
        return -1;
    memmove(tmp.buf, z->buf, z->size);
    memmove(tmp.buf + z->size, key->buf, key->size);
    if (sm3((void *)tmp.buf, tmp.size, out->buf) < 0)
        goto end;
    ret = 0;
end:
    free(tmp.buf);
    return ret;
}

static struct master_key_pair *generate_encrypt_master_key_pair(void) {
    struct master_key_pair *pair = NEW1(struct master_key_pair);
    if (!pair) {
        report_error(SM9_ERROR_OTHER);
        return NULL;
    }
    pair->type = TYPE_ENCRYPT;
    init_big(pair->priv);
    init_epoint(pair->pub);
#ifdef CHECK
    const char r[] = {0x01, 0xED, 0xEE, 0x37, 0x78, 0xF4, 0x41, 0xF8, 0xDE, 0xA3, 0xD9, 0xFA, 0x0A, 0xCC, 0x4E, 0x07, 0xEE, 0x36, 0xC9, 0x3F, 0x9A, 0x08, 0x61, 0x8A, 0xF4, 0xAD, 0x85, 0xCE, 0xDE, 0x1C, 0x22};
    read_big(pair->priv, r, sizeof(r));
#else
    bigrand(sm9_parameter.param_N, pair->priv);
#endif
    ecurve_mult(pair->priv, sm9_parameter.param_p1, pair->pub);
    return pair;
}

struct master_key_pair *generate_master_key_pair(enum GEN_TYPE type) {
    if (!sm9_is_init()) {
        report_error(SM9_ERROR_NOT_INIT);
        return NULL;
    }
    switch (type) {
        case TYPE_ENCRYPT:
            return generate_encrypt_master_key_pair();
    }
    return NULL;
}

static int T2(struct master_key_pair *pair, const struct _string *id,
        uint8_t hid, big res) {
    int ret = -1;
    big h1, t1, zero, t2, rem;
    if (H1(id, hid, &h1) < 0)
        return -1;
    init_big(t1);
    init_big(zero);
    init_big(t2);
    init_big(rem);
    add(h1, pair->priv, t1);
    if (!mr_compare(t1, zero)) {
        report_error(SM9_ERROR_KGC_GENPRIKEY_T1_IS_ZERO);
        goto end;
    }
    xgcd(t1, sm9_parameter.param_N, t1, t1, t1);
    multiply(pair->priv, t1, t2);
    divide(t2, sm9_parameter.param_N, rem);
    copy(t2, res);
    ret = 0;
end:
    release_big(t1);
    release_big(zero);
    release_big(t2);
    release_big(rem);
    return ret;
}

static struct private_key *get_encrypt_private_key(struct master_key_pair *pair,
        const struct _string *id, uint8_t hid) {
    big t2;
    struct private_key *priv = NEW1(struct private_key);
    if (!priv) {
        report_error(SM9_ERROR_OTHER);
        return NULL;
    }
    priv->type = TYPE_ENCRYPT;
    init_big(t2);
    init_ecn2(priv->e);
    if (T2(pair, id, hid, t2) < 0) {
        goto free_priv;
        return NULL;
    }
    ecn2_copy(&sm9_parameter.param_p2, &priv->e);
    ecn2_mul(t2, &priv->e);
    release_big(t2);
    return priv;
free_priv:
    release_big(t2);
    release_ecn2(priv->e);
    free(priv);
    return NULL;
}

struct private_key *get_private_key(struct master_key_pair *pair,
        const char *id, size_t idlen, enum GEN_TYPE type) {
    if (!sm9_is_init()) {
        report_error(SM9_ERROR_NOT_INIT);
        return NULL;
    }
    const struct _string idstr = get_string(id, idlen);
    switch (type) {
        case TYPE_ENCRYPT:
            return get_encrypt_private_key(pair, &idstr, HID_ENCRYPT);
    }
    return NULL;
}

static int get_qb(const struct master_key_pair *pair, const struct _string *id,
        int hid, epoint *qb) {
    int ret = -1;
    big h1;
    if (H1(id, hid, &h1) < 0)
        goto end;
    ecurve_mult(h1, sm9_parameter.param_p1, qb);
    ecurve_add(pair->pub, qb);
    release_big(h1);
    ret = 0;
end:
    return ret;
}

static int kdf(const struct _string *c1, struct zzn12 *w, struct _string *id, size_t klen,
        struct _string *res) {
    struct _string buf;
    struct _string ret;
    size_t s;
    if (!NEW_STRING(&buf, c1->size + zzn12_size(w) +
                id->size + KDF_EXTRA_BYTE_LEN))
        return -1;
    memmove(buf.buf, c1->buf, c1->size);
    s = c1->size;
    s += zzn12_to_string(w, buf.buf + s, buf.size - s);
    memmove(buf.buf + s, id->buf, id->size);
    if (KDF(&buf, klen, &ret) < 0) {
        free(buf.buf);
        return -1;
    }
    *res = ret;
    free(buf.buf);
    return 0;
}

static int handle_block(const struct _string *kdf, const struct _string *data,
        struct _string *k2str, struct _string *c2str,
        size_t k1size, size_t k2size, int encrypt) {
    struct _string k1 = get_string(kdf->buf, k1size);
    struct _string k2 = get_string(kdf->buf + k1size, k2size);
    if (is_zero(&k1))
        return 1;
    sm4_cbc(k1.buf, NULL, data->size, NULL, &c2str->size, encrypt);
    if (!NEW_STRING(c2str, c2str->size))
        return -1;
    if (sm4_cbc(k1.buf, (void *)data->buf, data->size,
                c2str->buf, &c2str->size, encrypt) < 0) {
        free(c2str->buf);
        return -1;
    }
    *k2str = k2;
    return 0;
}

static int handle_stream(const struct _string *kdf, const struct _string *data,
        struct _string *k2str, struct _string *c2str,
        size_t k1size, size_t k2size) {
    struct _string k1 = get_string(kdf->buf, k1size);
    struct _string k2 = get_string(kdf->buf + k1size, k2size);
    if (is_zero(&k1))
        return 1;
    if (!NEW_STRING(c2str, data->size))
        return -1;
    for (size_t i = 0; i < data->size; ++i)
        c2str->buf[i] = data->buf[i] ^ k1.buf[i];
    *k2str = k2;
    return 0;
}

static int get_c1(big r, epoint *qb, struct _string *c1) {
    int ret = -1;
    epoint *res;
    init_epoint(res);
    ecurve_mult(r, qb, res);
    if (write_epoint(res, c1) < 0)
        goto end;
    ret = 0;
end:
    release_epoint(res);
    return ret;
}

struct cipher *sm9_encrypt(
        struct master_key_pair *pair,
        const char *id, size_t idlen,
        const char *data, size_t datalen,
        int isblockcipher, int maclen) {
    int ret = -1;
    epoint *qb;
    big r;
    struct _string idstr = get_string(id, idlen);
    struct _string datastr = get_string(data, datalen);
    struct _string k, k2;
    struct zzn12 *g = NULL;
    struct cipher *cipher = NULL;

    if (!sm9_is_init())
        return NULL;
    init_epoint(qb);
    init_big(r);
    g = zzn12_get();
    k.buf = NULL;
    if (!(cipher = NEWZ1(struct cipher)))
        goto end;
    if (get_qb(pair, &idstr, HID_ENCRYPT, qb) < 0)
        goto end;
    while (1) {
#ifdef CHECK
        const char rs[] = {0xAA, 0xC0, 0x54, 0x17, 0x79, 0xC8, 0xFC, 0x45, 0xE3, 0xE2, 0xCB, 0x25, 0xC1, 0x2B, 0x5D, 0x25, 0x76, 0xB2, 0x12, 0x9A, 0xE8, 0xBB, 0x5E, 0xE2, 0xCB, 0xE5, 0xEC, 0x9E, 0x78, 0x5C};
        read_big(r, rs, sizeof(rs));
#else
        bigrand(sm9_parameter.param_N, r);
#endif
        if (get_c1(r, qb, &cipher->c1) < 0)
            goto end;
        if (!zzn12_calc_rate_pairing(g, sm9_parameter.param_p2, pair->pub,
                    sm9_parameter.param_t, sm9_parameter.norm_x))
            goto end;
        zzn12_pow(g, r);
        if (isblockcipher) {
            int ret = 0;
            if (kdf(&cipher->c1, g, &idstr, SM4_BLOCK_BIT_SIZE + maclen, &k) < 0)
                goto end;
            if ((ret =handle_block(&k, &datastr, &k2, &cipher->c2, SM4_BLOCK_BYTE_SIZE,
                        maclen >> 3, 1)) < 0)
                goto end;
            else if (!ret)
                break;
        }
        else {
            int ret = 0;
            if (kdf(&cipher->c1, g, &idstr,  (datalen << 3) + maclen, &k) < 0)
                goto end;
            if ((ret =handle_stream(&k, &datastr, &k2, &cipher->c2, datalen,
                        maclen >> 3)) < 0)
                goto end;
            else if (!ret)
                break;
        }
        free(k.buf);
        free(cipher->c1.buf);
        free(cipher->c2.buf);
        cipher->c1.buf = NULL;
        cipher->c2.buf = NULL;
    }
    if (!NEW_STRING(&cipher->c3, SM3_BYTE_SIZE))
        goto end;
    if (mac(&k2, &cipher->c2, &cipher->c3) < 0)
        goto end;
    ret = 0;
end:
    release_epoint(qb);
    release_big(r);
    zzn12_free(g);
    free(k.buf);
    if (ret < 0) {
        free(cipher->c1.buf);
        free(cipher->c2.buf);
        free(cipher->c3.buf);
        free(cipher);
        cipher = NULL;
    }
    return cipher;
}

int sm9_decrypt(
        struct private_key *priv,
        const struct cipher *cipher,
        const char *id, size_t idlen,
        int isblockcipher, int maclen,
        char **out, size_t *outlen) {
    int ret = -1;
    epoint *c1;
    struct zzn12 *w;
    struct _string idstr = get_string(id, idlen);
    struct _string k, m, k2, u;
    init_epoint(c1);
    w = zzn12_get();
    k.buf = NULL;
    m.buf = NULL;
    u.buf = NULL;
    if (!sm9_is_init())
        goto end;
    if (read_epoint(c1, cipher->c1.buf, cipher->c1.size) < 0)
        goto end;
    if (!is_point_on_g1(c1))
        goto end;
    if (!zzn12_calc_rate_pairing(w, priv->e, c1,
                sm9_parameter.param_t, sm9_parameter.norm_x))
        goto end;
    if (isblockcipher) {
        if (kdf(&cipher->c1, w, &idstr, SM4_BLOCK_BIT_SIZE + maclen, &k) < 0)
            goto end;
        if (handle_block(&k, &cipher->c2, &k2, &m, SM4_BLOCK_BYTE_SIZE,
                        maclen >> 3, 0))
            goto end;
    }
    else {
        if (kdf(&cipher->c1, w, &idstr,  (cipher->c2.size << 3) + maclen, &k) < 0)
            goto end;
        if (handle_stream(&k, &cipher->c2, &k2, &m, cipher->c2.size,
                        maclen >> 3))
            goto end;
    }
    if (!NEW_STRING(&u, SM3_BYTE_SIZE))
        goto end;
    if (mac(&k2, &cipher->c2, &u) < 0)
        goto end;
    if (memcmp(u.buf, cipher->c3.buf, cipher->c3.size))
        goto end;
    ret = 0;
end:
    release_epoint(c1);
    zzn12_free(w);
    free(k.buf);
    free(u.buf);
    if (ret < 0)
        free(m.buf);
    else {
        *out = (void *)m.buf;
        *outlen = m.size;
    }
    return ret;
}
