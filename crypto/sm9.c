#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
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
    if (inlen <= 96)
        return NULL;
    cipher = NEWZ1(struct cipher);
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
    if (ciphertext_size(cipher) > outlen)
        return 0;
    memmove(out, cipher->c1.buf, cipher->c1.size);
    memmove(out + cipher->c1.size, cipher->c3.buf, cipher->c3.size);
    memmove(out + cipher->c1.size + cipher->c3.size, cipher->c2.buf, cipher->c2.size);
    return ciphertext_size(cipher);
}

void ciphertext_free(struct cipher *cipher) {
    if (cipher) {
        free(cipher->c1.buf);
        free(cipher->c2.buf);
        free(cipher->c3.buf);
        free(cipher);
    }
}

size_t private_key_size(const struct private_key *priv) {
    return ecn2_size(&priv->e) + 1;
}

struct private_key *private_key_read(const char *b, size_t blen) {
    struct private_key *priv = NULL;
    if (blen < 128 ||
            !(priv = NEW1(struct private_key)))
        return NULL;
    init_ecn2(priv->e);
    if (!read_ecn2_byte128(&priv->e, (void *)b)) {
        free(priv);
        priv = NULL;
    }
    if (priv)
        priv->type = b[128];
    return priv;
}

size_t private_key_write(struct private_key *priv, char *b, size_t blen) {
    if (blen < private_key_size(priv))
        return 0;
    size_t size = write_ecn2(&priv->e, b, blen);
    b[size] = priv->type;
    return size + 1;
}

size_t master_key_pair_size(const struct master_key_pair *pair) {
    struct _string x, y;
    write_big(pair->priv, &x);
    write_epoint(pair->pub, &y);
    size_t size = 3 + x.size + y.size;
    free(x.buf);
    free(y.buf);
    return size;
}

struct master_key_pair *master_key_pair_read(const char *buf, size_t len) {
    int ret = -1;
    size_t size;
    struct master_key_pair *pair = NEW1(struct master_key_pair);
    if (len < buf[0] + 1)
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
        free(pair);
        pair = NULL;
    }
    return pair;
}

size_t master_key_pair_write(const struct master_key_pair *pair, char *buf, size_t len) {
    struct _string s, t;
    size_t size;
    write_big(pair->priv, &s);
    write_epoint(pair->pub, &t);
    if (3 + s.size + t.size > len) {
        free(s.buf);
        free(t.buf);
        return 0;
    }
    buf[0] = s.size;
    memmove(buf + 1, s.buf, s.size);
    size = 1 + s.size;
    free(s.buf);
    buf[size++] = t.size;
    memmove(buf + size, t.buf, t.size);
    size += t.size;
    buf[size] = pair->type;
    return size + 1;
}
