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

struct master_key_pair *master_key_pair_read_file(int masterfd) {
    struct stat st;
    char *buf = MAP_FAILED;
    struct master_key_pair *pair;
    if (masterfd < 0)
        return NULL;
    if (fstat(masterfd, &st) < 0 ||
        (buf = mmap(NULL, st.st_size,
                    PROT_READ, MAP_PRIVATE, masterfd, 0)) == MAP_FAILED)
        return NULL;
    if ((pair = master_key_pair_read(buf, st.st_size))) {
        munmap(buf, st.st_size);
        return pair;
    }
    munmap(buf, st.st_size);
    return NULL;
}

int master_key_pair_write_file(struct master_key_pair *pair, int fd) {
    size_t size = master_key_pair_size(pair);
    char *buf = g_new(char, size);
    master_key_pair_write(pair, buf, size);
    write(fd, buf, size);
    g_free(buf);
    return 0;
}

struct crypto *crypto_read_file(int fd) {
    int ret = -1;
    struct crypto_file *fp = NULL;
    struct crypto *crypto = NULL;
    struct stat st;
    if (fstat(fd, &st) < 0)
        goto end;
    if (st.st_size < (signed)sizeof(struct crypto_file))
        goto end;
    if ((fp = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
        goto end;
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
    if (fp != MAP_FAILED)
        munmap(fp, st.st_size);
    if (ret < 0) {
        crypto_free(crypto);
        crypto = NULL;
    }
    return crypto;
}

int crypto_write_file(struct crypto *crypto, int fd) {
    size_t idlen = crypto->idlen;
    size_t size = sizeof(struct crypto_file) + idlen - 1;
    struct crypto_file *c = g_malloc(size);
    c->idlen = idlen;
    memmove(c->id, crypto->id, idlen);
    private_key_write(crypto->pkey, c->priv, sizeof(c->priv));
    write(fd, c, size);
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
