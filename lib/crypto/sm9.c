#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<gmodule.h>
#include"parameter.h"
#include"sm9_helper.h"

static int init_flag;

int sm9_init(void) {
    int res;
    if (init_flag)
        return 0;
    res = sm9_parameter_init();
    init_flag = !!res;
    return 0;
}

void sm9_release(void) {
    if (!init_flag)
        return;
    sm9_parameter_release();
    init_flag = 0;
}

int sm9_is_init(void) {
    return init_flag;
}

struct cipher *ciphertext_read(const char *in, size_t inlen) {
    struct cipher *cipher = NULL;
    if (!in || inlen <= 96)
        return NULL;
    cipher = g_new0(struct cipher, 1);
    if (!cipher)
        return NULL;
    if (!NEW_STRING(&cipher->c1, 64) ||
            !NEW_STRING(&cipher->c3, 32) ||
            !NEW_STRING(&cipher->c2, inlen - 96))
        goto end;
    memmove(cipher->c1.buf, in, 64);
    memmove(cipher->c3.buf, in + 64, 32);
    memmove(cipher->c2.buf, in + 96, inlen - 96);
    return cipher;
end:
    ciphertext_free(cipher);
    return NULL;
}

size_t ciphertext_size(const struct cipher *cipher) {
    if (!cipher)
        return 0;
    return cipher->c1.size + cipher->c2.size + cipher->c3.size;
}

size_t ciphertext_write(const struct cipher *cipher,
        char *out, size_t outlen) {
    if (!out || ciphertext_size(cipher) > outlen)
        return 0;
    memmove(out, cipher->c1.buf, cipher->c1.size);
    memmove(out + cipher->c1.size, cipher->c3.buf, cipher->c3.size);
    memmove(out + cipher->c1.size + cipher->c3.size, cipher->c2.buf, cipher->c2.size);
    return ciphertext_size(cipher);
}

void ciphertext_free(struct cipher *cipher) {
    if (cipher) {
        g_free(cipher->c1.buf);
        g_free(cipher->c2.buf);
        g_free(cipher->c3.buf);
        g_free(cipher);
    }
}

size_t private_key_size(const struct private_key *priv) {
    return priv ? ecn2_size(&priv->e) + 1 : 0;
}

struct private_key *private_key_read(const char *b, size_t blen) {
    struct private_key *priv = NULL;
    if (!b || blen < 129 ||
            !(priv = g_new(struct private_key, 1)))
        return NULL;
    init_ecn2(priv->e);
    if (!read_ecn2_byte128(&priv->e, (void *)b)) {
        g_free(priv);
        priv = NULL;
    }
    if (priv)
        priv->type = b[128];
    return priv;
}

size_t private_key_write(struct private_key *priv, char *b, size_t blen) {
    if (!priv || !b || blen < private_key_size(priv))
        return 0;
    size_t size = write_ecn2(&priv->e, b, blen);
    b[size] = priv->type;
    return size + 1;
}

size_t master_key_pair_size(const struct master_key_pair *pair) {
    return pair ? 3 + big_size(pair->priv) + epoint_size(pair->pub) : 0;
}

struct master_key_pair *master_key_pair_read(const char *buf, size_t len) {
    int ret = -1;
    size_t size;
    struct master_key_pair *pair = g_new(struct master_key_pair, 1);
    if (!buf || !pair || len < (size_t)buf[0] + 1)
        goto end;
    init_big(pair->priv);
    init_epoint(pair->pub);
    read_big(pair->priv, buf + 1, buf[0]);
    size = 1 + buf[0];
    if (len < size + buf[size] + 1)
        goto end;
    read_epoint(pair->pub, (void *)(buf + size + 1), buf[size]);
    size += buf[size];
    if (len < size + 1)
        goto end;
    pair->type = buf[size];
    ret = 0;
end:
    if (ret < 0) {
        g_free(pair);
        pair = NULL;
    }
    return pair;
}

size_t master_key_pair_write(const struct master_key_pair *pair, char *buf, size_t len) {
    struct _string s, t;
    size_t size;
    if (!pair || !buf)
        return 0;
    write_big(pair->priv, &s);
    write_epoint(pair->pub, &t);
    if (3 + s.size + t.size > len) {
        g_free(s.buf);
        g_free(t.buf);
        return 0;
    }
    buf[0] = s.size;
    memmove(buf + 1, s.buf, s.size);
    size = 1 + s.size;
    g_free(s.buf);
    buf[size++] = t.size;
    memmove(buf + size, t.buf, t.size);
    g_free(t.buf);
    size += t.size;
    buf[size] = pair->type;
    return size + 1;
}

void master_key_pair_free(struct master_key_pair *pair) {
    release_big(pair->priv);
    release_epoint(pair->pub);
    g_free(pair);
}

void private_key_free(struct private_key *key) {
    release_ecn2(key->e);
    g_free(key);
}

