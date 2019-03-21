#include<string.h>
#include<sys/stat.h>
#include<stdio.h>
#include<errno.h>
#include<unistd.h>
#include<assert.h>
#include<sys/mman.h>
#include<openssl/evp.h>
#include<openssl/pem.h>
#include<openssl/err.h>
#include"context.h"
#include"config.h"

struct user_args {
    const char *block_device;
    const char *pkey_file;
    const char *key_file;
    int show_help;
};

struct block_header {
    uint8_t header_number;
    uint8_t reserve[255];
    uint8_t (*ciphertext)[256];
} __attribute__((packed));

#define OPTION(t, p) \
    { t, offsetof(struct user_args, p), 1 }
#define STR_EMPTY(str) (!(str) || *(str) == '\0')
#define NEW(type, n) ((type *)calloc(n, sizeof(type)))

static int decrypt(EVP_PKEY *pkey, const uint8_t *ciphertext, uint8_t **plain, size_t *out_len) {
    static uint8_t buf[256];
    static size_t len = 256;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return -1;
    if (EVP_PKEY_decrypt_init(ctx) <= 0)
        goto free_ctx;
    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
    if (EVP_PKEY_decrypt(ctx, buf, &len, ciphertext, 256) <= 0)
        goto free_ctx;
    if (plain)
        *plain = buf;
    if (out_len)
        *out_len = len;
    EVP_PKEY_CTX_free(ctx);
    return 0;
free_ctx:
    EVP_PKEY_CTX_free(ctx);
    return -1;
}

static EVP_PKEY *get_pkey(const char *pkey_file, const char *key_file) {
    int keyfd;
    struct stat st;
    unsigned char *key;
    BIO *bio;
    EVP_PKEY *pkey;
    if ((keyfd = open(key_file, O_RDONLY)) < 0 ||
            fstat(keyfd, &st) < 0 ||
            (key = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, keyfd, 0)) == MAP_FAILED) {
        if (keyfd > 0)
            close(keyfd);
        fprintf(stderr, "%s access error: %s\n", key_file, strerror(errno));
        return NULL;
    }
    if (!(bio = BIO_new_file(pkey_file, "r")))
        goto free_fd;
    PEM_read_bio_PrivateKey(bio, &pkey, NULL, key);
    if (!pkey)
        goto free_fd;
    munmap(key, st.st_size);
    close(keyfd);
    return pkey;
free_fd:
    munmap(key, st.st_size);
    close(keyfd);
    return NULL;
}

int check_header(struct block_header *header, struct user_args *args,
        struct encfs_context *context) {
    int i;
    context->iv = NEW(unsigned char, context->ivsize);
    context->key = NEW(unsigned char, context->keysize);
    if (!context->iv || !context->key) {
        free(context->iv);
        return -1;
    }
    EVP_PKEY *pkey = get_pkey(args->pkey_file, args->key_file);
    if (!pkey)
        goto free_buf;
    for (i = 0; i < header->header_number; ++i) {
        uint8_t *ciphertext = header->ciphertext[i];
        uint8_t *plain;
        size_t size;
        if (!decrypt(pkey, ciphertext, &plain, &size)) {
            if ((int)size != context->ivsize + context->keysize)
                continue;
            memmove(context->key, plain, context->keysize);
            memmove(context->iv, plain + context->keysize, context->ivsize);
            EVP_PKEY_free(pkey);
            return 0;
        }
    }
    EVP_PKEY_free(pkey);
    return -1;
free_buf:
    free(context->iv);
    free(context->key);
    context->iv = NULL;
    context->key = NULL;
    return -1;
}

void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --help                   print this help message.\n", stderr);
    fputs("    --block-device=[device]  set target block device.\n", stderr);
    fputs("    --key=[keyfile]          passphrase for private key.\n", stderr);
    fputs("    --pkey=[pkeyfile]        private key for block device.\n", stderr);
    fuse_opt_add_arg(args, "--help");
}

int check_args(struct encfs_context *context, struct user_args *args,
        struct fuse_args *fuse_args) {
    uint8_t size;
    struct block_header *header = NULL;
    struct stat blkst;
    if (STR_EMPTY(args->block_device) ||
            STR_EMPTY(args->key_file) ||
            STR_EMPTY(args->pkey_file)) {
        usage(fuse_args);
        return -1;
    }
    if (pthread_mutex_init(&context->mutex, NULL) < 0) {
        perror("encfs error");
        return -1;
    }
    context->ctx = EVP_CIPHER_CTX_new();
    if (!context->ctx)
        goto free_lock;
    context->cipher = EVP_aes_256_xts();
    context->ivsize = EVP_CIPHER_iv_length(context->cipher);
    context->keysize = EVP_CIPHER_key_length(context->cipher);
    context->blkfd = open(args->block_device, O_RDWR);
    if (context->blkfd < 0) {
        fprintf(stderr, "%s open error: %s\n", args->block_device,
                strerror(errno));
        goto free_ctx;
    }
    if (fstat(context->blkfd, &blkst) < 0) {
        fprintf(stderr, "%s stat error: %s\n", args->block_device,
                strerror(errno));
        goto free_blk;
    }
    context->block_size = blkst.st_size;
    if (write(context->blkfd, &size, 1) <= 1) {
        fprintf(stderr, "%s read error", args->block_device);
        goto free_blk;
    }
    size = (size + 1) * 256;
    if (context->block_size < size) {
        fprintf(stderr, "%s read error: %s\n", args->block_device,
                strerror(ENOSPC));
        goto free_blk;
    }
    if ((header = mmap(NULL, size, PROT_READ, MAP_PRIVATE,
                    context->blkfd, 0)) == MAP_FAILED) {
        fprintf(stderr, "%s read error: %s\n", args->block_device,
                strerror(errno));
        goto free_blk;
    }
    context->start_offset = size;
    if (check_header(header, args, context) < 0)
        goto free_map;
    munmap(header, size);
    return 0;
free_map:
    munmap(header, size);
free_blk:
    close(context->blkfd);
free_ctx:
    EVP_CIPHER_CTX_free(context->ctx);
free_lock:
    pthread_mutex_destroy(&context->mutex);
    return -1;
}

struct encfs_context *encfs_context_init(struct fuse_args *fuse_args) {
    const struct fuse_opt option_spec[] = {
        OPTION("--block-device=%s", block_device),
        OPTION("--key=%s", key_file),
        OPTION("--pkey=%s", pkey_file),
        OPTION("--help", show_help),
        FUSE_OPT_END
    };
    struct user_args args = {};
    struct encfs_context *context = NEW(struct encfs_context, 1);
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
    free(context->iv);
    free(context->key);
    EVP_CIPHER_CTX_free(context->ctx);
    pthread_mutex_unlock(&context->mutex);
    pthread_mutex_destroy(&context->mutex);
}

