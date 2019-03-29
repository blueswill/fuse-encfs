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
    return ecn2_size(&priv->e);
}

struct private_key *private_key_read(const char *b, size_t blen) {
    struct private_key *priv = NULL;
    if (blen < 128 ||
            !(priv = NEW1(struct private_key)))
        return NULL;
    if (!read_ecn2_byte128(&priv->e, (void *)b)) {
        free(priv);
        priv = NULL;
    }
    return priv;
}

static size_t big_write_ecn2(big t, char *b, size_t blen) {
    big r;
    init_big(r);
    redc(t, r);
    size_t ret = write_big_buf(r, b, blen);
    release_big(r);
    return ret;
}

size_t private_key_write(const struct private_key *priv, char *b, size_t blen) {
    size_t r;
    if (blen < private_key_size(priv))
        return 0;
    r = big_write_ecn2(priv->e.x.b, b, blen);
    r += big_write_ecn2(priv->e.x.a, b + r, blen - r);
    r += big_write_ecn2(priv->e.y.b, b + r, blen - r);
    r += big_write_ecn2(priv->e.y.a, b + r, blen - r);
    return r;
}
