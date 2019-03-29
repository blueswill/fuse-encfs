#include<string.h>
#include<sys/stat.h>
#include<stdio.h>
#include<errno.h>
#include<unistd.h>
#include<assert.h>
#include<sys/mman.h>
#include<sys/ioctl.h>
#include<linux/fs.h>
#include"context.h"
#include"config.h"
#include"encfs_helper.h"
#include"sm9.h"
#include"sm4.h"

struct user_args {
    const char *block_device;
    const char *pkey_file;
    int show_help;
};

struct crypto {
    struct private_key *pkey;
    char *id;
    size_t idlen;
};

struct crypto_file {
    uint8_t priv[128];
    uint8_t idlen;
    uint8_t id[1];
} __attribute__((packed));

struct block_header {
    uint8_t header_flag;
    uint8_t header_number;
    uint8_t ciphertext[3][160];
    uint8_t reserve[30];
} __attribute__((packed));

#define HEADER_CUR_TYPE 0x1
#define HEADER_NXT_TYPE 0x2
#define IS_HEADER(flag, type) ((flag) & (type))

#define OPTION(t, p) \
    { t, offsetof(struct user_args, p), 1 }
#define STR_EMPTY(str) (!(str) || *(str) == '\0')

static void crypto_free(struct crypto *c) {
    if (c) {
        private_key_free(c->pkey);
        free(c->id);
    }
}

static int decrypt(struct crypto *crypto,
        const uint8_t *ciphertext, size_t cipherlen,
        uint8_t **plain, size_t *out_len) {
    int ret = -1;
    struct cipher *cipher = ciphertext_read((void *)ciphertext, cipherlen);
    if (!cipher)
        goto end;
    ret = sm9_decrypt(crypto->pkey, cipher,
            crypto->id, crypto->idlen, 1, 0x100,
            (void *)plain, out_len);
    ret = 0;
end:
    ciphertext_free(cipher);
    return ret;
}

static struct crypto *get_pkey(const char *pkey_file) {
    int fd = -1, ret = -1;
    struct crypto_file *fp = NULL;
    struct crypto *crypto = NULL;
    struct stat st;
    if ((fd = open(pkey_file, O_RDONLY)) < 0)
        goto end;
    if (fstat(fd, &st) < 0)
        goto end;
    if (st.st_size < (signed)sizeof(struct crypto_file))
        goto end;
    if ((fp = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
        goto end;
    if (!(crypto = NEWZ1(struct crypto)))
        goto end;
    if (!(crypto->pkey = private_key_read(fp->priv, 128)))
        goto end;
    if (!(crypto->id = NEW(uint8_t, fp->idlen)))
        goto end;
    memmove(crypto->id, fp->id, fp->idlen);
    ret = 0;
end:
    if (fd >= 0)
        close(fd);
    if (fp != MAP_FAILED)
        munmap(fp, st.st_size);
    if (ret < 0) {
        crypto_free(crypto);
    }
    return crypto;
}

int check_header(struct block_header *header, struct user_args *args,
        struct encfs_context *context) {
    int ret = -1;
    context->key = NEW(unsigned char, context->keysize);
    struct crypto *crypto = NULL;
    struct block_header *ptr = header;
    if (!context->key)
        goto end;
    if (!(crypto = get_pkey(args->pkey_file)))
        goto end;
    while (IS_HEADER(ptr->header_flag, HEADER_CUR_TYPE)) {
        int i;
        for (i = 0; i < ptr->header_number; ++i) {
            uint8_t *ciphertext = ptr->ciphertext[i];
            struct cipher *cipher = ciphertext_read((void *)ciphertext, 160);
            char *out;
            size_t outlen;
            if (cipher && !sm9_decrypt(crypto->pkey, cipher,
                        crypto->id, crypto->idlen, 1, 0x100,
                        &out, &outlen) &&
                    outlen >= context->keysize) {
                memmove(context->key, out, context->keysize);
                ret = 0;
                goto end;
            }
            free(out);
            ciphertext_free(cipher);
        }
        if (!IS_HEADER(ptr->header_flag, HEADER_NXT_TYPE))
            goto end;
        ++ptr;
    }
    context->start_offset = sizeof(struct block_header) * (ptr - header);
end:
    if (ret < 0) {
        free(context->key);
        crypto_free(crypto);
        context->key = NULL;
    }
    return ret;
}

void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --help                   print this help message.\n", stderr);
    fputs("    --block-device=[device]  set target block device.\n", stderr);
    fputs("    --pkey=[pkeyfile]        private key for block device.\n", stderr);
    fuse_opt_add_arg(args, "--help");
}

static int get_file_size(int fd, size_t *size) {
    struct stat st;
    long long s;
    if (fstat(fd, &st) < 0)
        return -1;
    if (S_ISBLK(st.st_mode)) {
        if (ioctl(fd, BLKGETSIZE64, &s) < 0)
            return -1;
        *size = s;
    }
    else if (S_ISREG(st.st_mode))
        *size = st.st_size;
    else
        return -1;
    return 0;
}

int check_args(struct encfs_context *context, struct user_args *args,
        struct fuse_args *fuse_args) {
    int ret = -1;
    struct block_header *header = NULL;
    if (STR_EMPTY(args->block_device) ||
            STR_EMPTY(args->pkey_file)) {
        usage(fuse_args);
        return -1;
    }
    context->keysize = SM4_XTS_KEY_BYTE_SIZE;
    context->blkfd = open(args->block_device, O_RDWR);
    if (context->blkfd < 0)
        goto end;
    if (get_file_size(context->blkfd, &context->block_size) < 0)
        goto end;
    if (FLOOR_OFFSET2(context->block_size, SECTOR_SHIFT))
        goto end;
    if ((header = mmap(NULL, context->block_size, PROT_READ, MAP_PRIVATE,
                    context->blkfd, 0)) == MAP_FAILED)
        goto end;
    if (check_header(header, args, context) < 0)
        goto end;
    pthread_mutex_init(&context->mutex, NULL);
    ret = 0;
end:
    if (ret < 0) {
        if (context->blkfd)
            close(context->blkfd);
    }
    if (header != MAP_FAILED)
        munmap(header, context->block_size);
    return ret;
}

struct encfs_context *encfs_context_init(struct fuse_args *fuse_args) {
    const struct fuse_opt option_spec[] = {
        OPTION("--block-device=%s", block_device),
        OPTION("--pkey=%s", pkey_file),
        OPTION("--help", show_help),
        FUSE_OPT_END
    };
    struct user_args args = {};
    struct encfs_context *context = NEWZ1(struct encfs_context);
    if (!context)
        return NULL;
    if (fuse_opt_parse(fuse_args, &args, option_spec, NULL) == -1 ||
            check_args(context, &args, fuse_args)) {
        free(context);
        return NULL;
    }
    return context;
}

void encfs_context_free(struct encfs_context *context) {
    pthread_mutex_lock(&context->mutex);
    close(context->blkfd);
    free(context->key);
    pthread_mutex_unlock(&context->mutex);
    pthread_mutex_destroy(&context->mutex);
}

