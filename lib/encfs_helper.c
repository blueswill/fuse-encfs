#include<sys/stat.h>
#include<unistd.h>
#include<sys/mman.h>
#include<string.h>
#include<stdint.h>
#include<gmodule.h>
#include"encfs_helper.h"
#include"config.h"

struct crypto_file {
    char priv[129];
    uint8_t idlen;
    uint8_t id[1];
} __attribute__((packed));

struct master_key_pair *master_key_pair_read_file(int masterfd, ENCRYPT_DECRYPT_FUNC func, void *userdata) {
    struct stat st;
    char *buf = MAP_FAILED;
    struct master_key_pair *pair = NULL;
    char *res;
    size_t size;
    if (masterfd < 0)
        return NULL;
    if (fstat(masterfd, &st) < 0 ||
        (buf = mmap(NULL, st.st_size,
                    PROT_READ, MAP_PRIVATE, masterfd, 0)) == MAP_FAILED)
        return NULL;
    res = buf;
    size = st.st_size;
    if (func && func(res, size, &res, &size, userdata) < 0)
        goto end;
    pair = master_key_pair_read(res, size);
end:
    if (res != buf)
        g_free(res);
    munmap(buf, st.st_size);
    return pair;
}

int master_key_pair_write_file(struct master_key_pair *pair, int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata) {
    size_t size = master_key_pair_size(pair);
    char *buf = g_new(char, size);
    master_key_pair_write(pair, buf, size);
    if (func) {
        char *tmp = buf;
        if (func(buf, size, &buf, &size, userdata) < 0) {
            g_free(tmp);
            return -1;
        }
        g_free(tmp);
    }
    write(fd, buf, size);
    return 0;
}

static struct crypto_file *_get_crypto_file(int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata) {
    char *mapped = MAP_FAILED;
    size_t size;
    struct crypto_file *fp = NULL;
    struct stat st;
    int flag = 1;
    if (fstat(fd, &st) < 0)
        goto end;
    size = st.st_size;
    if ((mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
        goto end;
    if (func) {
        char *buf;
        size_t s;
        if (func(mapped, size, &buf, &s, userdata) < 0)
            goto end;
        if (s < sizeof(struct crypto_file)) {
            g_free(buf);
            goto end;
        }
        munmap(mapped, size);
        mapped = buf;
        size = s;
        flag = 0;
    }
end:
    if (flag && mapped != MAP_FAILED)
        munmap(mapped, size);
    else
        g_free(mapped);
    return fp;
}

struct crypto *crypto_read_file(int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata) {
    struct crypto_file *fp = _get_crypto_file(fd, func, userdata);
    struct crypto *crypto = NULL;
    int ret = -1;
    if (!fp)
        return NULL;
    if (!(crypto = g_new0(struct crypto, 1)))
        goto end;
    if (!(crypto->pkey = private_key_read(fp->priv, sizeof(fp->priv))))
        goto end;
    crypto->idlen = fp->idlen;
    if (!(crypto->id = g_new(char, crypto->idlen)))
        goto end;
    memmove(crypto->id, fp->id, crypto->idlen);
    ret = 0;
end:
    if (ret < 0) {
        crypto_free(crypto);
        crypto = NULL;
    }
    return crypto;
}

int crypto_write_file(struct crypto *crypto, int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata) {
    size_t idlen = crypto->idlen;
    size_t size = sizeof(struct crypto_file) + idlen - 1;
    struct crypto_file *c = g_malloc(size);
    c->idlen = idlen;
    memmove(c->id, crypto->id, idlen);
    private_key_write(crypto->pkey, c->priv, sizeof(c->priv));
    if (func) {
        char *buf;
        size_t s;
        if (func((void *)c, size, &buf, &s, userdata) < 0)
            goto end;
        g_free(c);
        c = (void *)buf;
        size = s;
    }
    write(fd, c, size);
end:
    g_free(c);
    return 0;
}

void crypto_free(struct crypto *crypto) {
    g_free(crypto->id);
    private_key_free(crypto->pkey);
    g_free(crypto);
}

struct crypto *crypto_new(struct master_key_pair *pair, const char *id) {
    struct crypto *c = g_new(struct crypto, 1);
    c->id = g_strdup(id);
    c->idlen = strlen(id);
    c->pkey = get_private_key(pair, id, c->idlen, TYPE_ENCRYPT);
    return c;
}
