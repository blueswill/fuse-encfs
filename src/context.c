#include<string.h>
#include<stdio.h>
#include<errno.h>
#include<unistd.h>
#include<assert.h>
#include"context.h"
#include"config.h"

struct user_args {
    const char *block_device;
    const char *key_file;
    const char *iv_file;
    int show_help;
};

#define offsetof(type, elem) \
    ((unsigned long)&(((type *)0)->elem))
#define OPTION(t, p) \
    { t, offsetof(struct user_args, p), 1 }
#define STR_EMPTY(str) (!(str) || *(str) == '\0')
#define NEW(type, n) ((type *)calloc(n, sizeof(type)))

void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --help                   print this help message.\n", stderr);
    fputs("    --block-device=[device]  set target block device.\n", stderr);
    fputs("    --key=[keyfile]          set aes-256-xts key for block device.\n", stderr);
    fputs("    --iv=[ivfile]            set aes-256-xts iv for block device.\n", stderr);
    assert(fuse_opt_add_arg(args, "--help") == 0);
}


int check_args(struct encfs_context *context, struct user_args *args,
        struct fuse_args *fuse_args) {
    int keyfd, ivfd;
    ssize_t rd;
    struct stat st;
    if (pthread_mutex_init(&context->mutex, NULL) < 0) {
        perror("encfs error");
        return -1;
    }
    if (!(context->ctx = EVP_CIPHER_CTX_new()))
        goto free_lock;
    context->cipher = EVP_aes_256_xts();
    context->ivsize = EVP_CIPHER_iv_length(context->cipher);
    context->keysize = EVP_CIPHER_key_length(context->cipher);
    context->iv = context->key = NULL;
    context->iv = NEW(unsigned char, context->ivsize);
    if (!context->iv || !(context->key = NEW(unsigned char, context->keysize))) {
        perror("encfs error");
        goto free_buf;
    }
    if (STR_EMPTY(args->block_device) ||
            STR_EMPTY(args->key_file) ||
            STR_EMPTY(args->iv_file)) {
        usage(fuse_args);
        goto free_buf;
    }
    if ((context->blkfd = open(args->block_device, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", args->block_device,
                strerror(errno));
        goto free_buf;
    }
    if ((keyfd = open(args->key_file, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", args->key_file,
                strerror(errno));
        goto free_blk;
    }
    if ((ivfd = open(args->iv_file, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", args->iv_file,
                strerror(errno));
        goto free_key;
    }
    if ((rd = read(keyfd, context->key, context->keysize)) < 0) {
        fprintf(stderr, "%s read error: %s\n", args->key_file,
                strerror(errno));
        goto free_iv;
    }
    if ((rd = read(ivfd, context->iv, context->ivsize)) < 0) {
        fprintf(stderr, "%s read error: %s\n", args->iv_file,
                strerror(errno));
        goto free_iv;
    }
    if (fstat(context->blkfd, &st) < 0) {
        fprintf(stderr, "%s fstat error: %s\n", args->block_device,
                strerror(errno));
        goto free_iv;
    }
    if (SECTOR_OFFSET(st.st_size) != 0) {
        fprintf(stderr, "size of %s is not valid\n", args->block_device);
        goto free_iv;
    }
    context->block_size = st.st_size;
    return 0;
free_iv:
    close(ivfd);
free_key:
    close(keyfd);
free_blk:
    close(context->blkfd);
free_buf:
    free(context->iv);
    free(context->key);
    free(context->ctx);
free_lock:
    pthread_mutex_destroy(&context->mutex);
    return -1;
}


struct encfs_context *encfs_context_init(struct fuse_args *fuse_args) {
    const struct fuse_opt option_spec[] = {
        OPTION("--block-device=%s", block_device),
        OPTION("--key=%s", key_file),
        OPTION("--iv=%s", iv_file),
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

