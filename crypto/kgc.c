#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include"sm9.h"
#include"parameter.h"
#include"sm9_helper.h"
#include"zzn12.h"
#include"sm4.h"

void master_key_pair_free(struct master_key_pair *pair) {
    release_big(pair->priv);
    release_epoint(pair->pub);
    free(pair);
}

void private_key_free(struct private_key *key) {
    release_ecn2(key->e);
    free(key);
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

struct cipher *sm9_encrypt(
        struct master_key_pair *pair,
        const char *id, size_t idlen,
        const char *data, size_t datalen,
        int isblockcipher, int maclen) {
    struct _string idstr = get_string(id, idlen);
    struct _string datastr = get_string(data, datalen);
    big h1, r;
    epoint *qb, *c1, *c2, *c3;
    struct zzn12 g, w;
    size_t k1len = SM4_BLOCK_BIT_SIZE;
    size_t k2len = maclen;
    size_t klen;
    struct _string str, kdf;
    uint8_t *k1, *k2;
    str.buf = NULL;
    kdf.buf = NULL;

    if (!sm9_is_init()) {
        report_error(SM9_ERROR_NOT_INIT);
        return NULL;
    }

    if (H1(&idstr, HID_ENCRYPT, &h1) < 0)
        return NULL;

    init_epoint(qb);
    init_epoint(c1);
    init_big(r);

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
        str.size = epoint_size(c1, NULL, NULL) + zzn12_size(w) +
            idlen + KDF_EXTRA_BYTE_LEN;
        str.buf = NEW(uint8_t, str.size);
        if (!str.buf) {
            report_error(SM9_ERROR_OTHER);
            goto end;
        }
        if (isblockcipher)
            klen = k1len + k2len;
        else
            klen = (datalen << 3) + k2len;
        size_t writed = write_epoint_buf(c1, str.buf, str.size);
        writed += zzn12_to_string(&w, str.buf + writed, str.size - writed);
        memmove(str.buf + writed, id, idlen);
        if (KDF(&str, klen, &kdf) < 0)
            goto end;
        k1 = str.buf;
        k2 = str.buf + ((klen - k2len) >> 3);
        if (!is_zero(k1, (klen - k2len) >> 3))
            break;
        free(str.buf);
        free(kdf.buf);
    }
    if (isblockcipher) {

    }
end:
    return NULL;
}
