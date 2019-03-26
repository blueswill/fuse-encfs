#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include"sm9.h"
#include"parameter.h"
#include"sm9_helper.h"
#include"zzn12.h"
#include"sm4.h"
#include"sm3.h"

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
    bigrand(sm9_parameter.param_N, pair->priv);
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
        uint8_t hid, big *res) {
    big h1, t1, zero, t2, rem;
    if (H1(id, hid, &h1) < 0)
        return -1;
    init_big(t1);
    init_big(zero);
    init_big(t2);
    init_big(rem);
    add(h1, pair->priv, t1);
    if (mr_compare(t1, zero)) {
        report_error(SM9_ERROR_KGC_GENPRIKEY_T1_IS_ZERO);
        goto end;
    }
    xgcd(t1, sm9_parameter.param_N, t1, t1, t1);
    multiply(pair->priv, t1, t2);
    divide(t2, sm9_parameter.param_N, rem);
    *res = rem;
    return 0;
end:
    release_big(t1);
    release_big(zero);
    release_big(t2);
    release_big(rem);
    return -1;
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
    if (T2(pair, id, hid, &t2) < 0) {
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
    const struct _string idstr = {
        .buf = (void *)id,
        .size = idlen
    };
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
    init_big(h1);
    if (H1(id, hid, &h1) < 0)
        goto end;
    ecurve_mult(h1, sm9_parameter.param_p1, qb);
    ecurve_add(pair->pub, qb);
    ret = 0;
end:
    release_big(h1);
    return ret;
}

static int get_c1(big r, epoint *qb, struct _string *c1) {
    epoint *c;
    init_epoint(c);
    ecurve_mult(r, qb, c);
}

struct cipher *sm9_encrypt(
        struct master_key_pair *pair,
        const char *id, size_t idlen,
        const char *data, size_t datalen,
        int isblockcipher, int maclen) {
    int ret = -1;
    struct _string idstr = get_string(id, idlen);
    big h1, r;
    epoint *qb, *c1;
    struct zzn12 g, w;
    size_t k1len = SM4_BLOCK_BIT_SIZE, k2len = maclen, klen;
    struct _string c1str, kdf, c2str, c3str, k1, k2;
    struct cipher *cipher = NULL;
    c1str.buf = NULL;
    kdf.buf = NULL;
    c2str.buf = NULL;
    c3str.buf = NULL;
    k1.buf = NULL;
    k2.buf = NULL;

    if (!sm9_is_init()) {
        report_error(SM9_ERROR_NOT_INIT);
        return NULL;
    }

    if (H1(&idstr, HID_ENCRYPT, &h1) < 0)
        return NULL;

    init_big(r);
    init_epoint(qb);
    init_epoint(c1);

    ecurve_mult(h1, sm9_parameter.param_p1, qb);
    ecurve_add(pair->pub, qb);
    while (1) {
        bigrand(sm9_parameter.param_N, r);
        ecurve_mult(r, qb, c1);
        if (zzn12_calcRatePairing(&g, sm9_parameter.param_p2, pair->pub,
                    sm9_parameter.param_t, sm9_parameter.norm_x)) {
            report_error(SM9_ERROR_CALC_RATE);
            goto end;
        }
        w = zzn12_pow(&g, r);
        if (!NEW_STRING(&c1str, epoint_size(c1, NULL, NULL) + zzn12_size(w) +
                    idlen + KDF_EXTRA_BYTE_LEN)) {
            report_error(SM9_ERROR_OTHER);
            goto end;
        }
        if (isblockcipher)
            klen = k1len + k2len;
        else
            klen = (datalen << 3) + k2len;
        size_t writed = write_epoint_buf(c1, c1str.buf, c1str.size);
        writed += zzn12_to_string(&w, c1str.buf + writed, c1str.size - writed);
        memmove(c1str.buf + writed, id, idlen);
        if (KDF(&c1str, klen, &kdf) < 0)
            goto end;
        k1.buf = kdf.buf;
        k1.size = (klen - k2len) >> 3;
        k2.buf = kdf.buf + k1.size;
        k2.size = k2len >> 3;
        if (!is_zero(&k1))
            break;
        free(c1str.buf);
        free(kdf.buf);
    }
    if (isblockcipher) {
        sm4_cbc(k1.buf, NULL, datalen, NULL, &c2str.size, 1);
        if (!(c2str.buf = NEW(uint8_t, c2str.size)))
            goto end;
        if (sm4_cbc(k1.buf, (void *)data, datalen,
                    c2str.buf, &c2str.size, 1) < 0)
            goto end;
    }
    else {
        if (!NEW_STRING(&c2str, datalen))
            goto end;
        for (size_t i = 0; i < datalen; ++i)
            c2str.buf[i] = data[i] ^ k1.buf[i];
    }
    if (!NEW_STRING(&c3str, SM3_BYTE_SIZE))
        goto end;
    if (mac(&k2, &c2str, &c3str) < 0)
        goto end;
    if (!(cipher = NEWZ1(struct cipher)))
        goto end;
    if (!NEW_STRING(&cipher->text, c1str.size + c2str.size + c3str.size))
        goto end;
    memmove(cipher->text.buf, c1str.buf, c1str.size);
    memmove(cipher->text.buf + c1str.size, c3str.buf, c3str.size);
    memmove(cipher->text.buf + c1str.size + c3str.size, c2str.buf, c2str.size);
    ret = 0;
end:
    release_big(h1);
    release_big(r);
    release_epoint(qb);
    release_epoint(c1);
    zzn12_release(&g);
    zzn12_release(&w);
    free(c1str.buf);
    free(c2str.buf);
    free(c3str.buf);
    free(kdf.buf);
    if (ret < 0) {
        if (cipher)
            free(cipher->text.buf);
        free(cipher);
        cipher = NULL;
    }
    return cipher;
}
