#include<string.h>
#include<unistd.h>
#include<stdarg.h>
#include<pthread.h>
#include<stdint.h>
#include<stropts.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<linux/fs.h>
#include<gmodule.h>
#include"mount-context.h"
#include"sm4.h"
#include"encfs-cb.h"

static int check_header(struct mount_context *ctx,
                        struct block_header *header,
                        struct crypto *crypto) {
    struct block_header *ptr = header;
    while (IS_HEADER(ptr->header_flag, HEADER_CUR_TYPE)) {
        int i;
        for (i = 0; i < ptr->header_number; ++i) {
            uint8_t *ciphertext = ptr->ciphertext[i];
            struct cipher *cipher = ciphertext_read((void *)ciphertext, 160);
            char *out = NULL;
            size_t outlen;
            if (cipher && !sm9_decrypt(crypto->pkey, cipher,
                                       crypto->id, crypto->idlen, 1, 0x100,
                                       &out, &outlen) &&
                outlen == ctx->keysize) {
                memmove(ctx->key, out, ctx->keysize);
                ctx->start_offset = sizeof(struct block_header) * (ptr - header + 1);
                free(out);
                free(cipher);
                return 0;
            }
            ciphertext_free(cipher);
        }
        if (!IS_HEADER(ptr->header_flag, HEADER_NXT_TYPE))
            return -1;
        ++ptr;
    }
}

struct mount_context *mount_context_new(int blkfd, struct crypto *crypto) {
    int ret = -1;
    struct block_header *header = MAP_FAILED;
    struct mount_context *ctx = g_new(struct mount_context, 1);
    ctx->blkfd = dup(blkfd);
    ctx->keysize = SM4_XTS_KEY_BYTE_SIZE;
    ctx->block_size = SM4_BLOCK_BYTE_SIZE;
    ctx->key = g_new(uint8_t, ctx->keysize);
    pthread_mutex_init(&ctx->mutex, NULL);
    ioctl(blkfd, BLKGETSIZE64, &ctx->block_size);
    if ((header = mmap(NULL, ctx->block_size, PROT_READ,
                       MAP_PRIVATE, blkfd, 0)) == MAP_FAILED)
        goto end;
    if (check_header(ctx, header, crypto) < 0)
        goto end;
    ret = 0;
end:
    if (ret < 0) {
        if (header != MAP_FAILED)
            munmap(header, ctx->block_size);
        mount_context_free(ctx);
        ctx = NULL;
    }
    return ctx;
}

void mount_context_free(struct mount_context *ctx) {
    if (ctx) {
        g_free(ctx->key);
        close(ctx->blkfd);
        g_free(ctx);
        pthread_mutex_destroy(&ctx->mutex);
    }
}

struct mount_context *mount_context_copy(struct mount_context *ctx) {
    struct mount_context *new = NULL;
    if (ctx) {
        new = g_new(struct mount_context, 1);
        new->blkfd = dup(ctx->blkfd);
        new->block_size = ctx->block_size;
        new->keysize = ctx->keysize;
        new->key = g_new(uint8_t, ctx->keysize);
        memmove(new->key, ctx->key, ctx->keysize);
        new->start_offset = ctx->start_offset;
        pthread_mutex_init(&new->mutex, NULL);
    }
    return new;
}

int mount_context_mount_raw(struct mount_context *ctx, struct fuse_args *args) {
    struct fuse_operations oprs = {
        .init = encfs_init,
        .getattr = encfs_getattr,
        .readdir = encfs_readdir,
        .open = encfs_open,
        .read = encfs_read,
        .write = encfs_write,
        .destroy = encfs_destroy
    };
    return fuse_main(args->argc, args->argv, &oprs, ctx);
}

int mount_context_mountv(struct mount_context *ctx, const char *mount_point,
                         int argc, va_list ap) {
    int ret;
    char **args = g_new(char*, argc + 3);
    args[0] = g_strdup("encfs");
    for (int i = 0; i < argc; ++i) {
        const char *ptr = va_arg(ap, const char *);
        args[i + 1] = g_strdup(ptr);
    }
    args[argc + 1] = g_strdup(mount_point);
    args[argc + 2] = NULL;
    struct fuse_args fuse_args = FUSE_ARGS_INIT(argc + 2, args);
    struct mount_context *new = mount_context_copy(ctx);
    if ((ret = mount_context_mount_raw(new, &fuse_args)))
        mount_context_free(new);
    fuse_opt_free_args(&fuse_args);
    g_strfreev(args);
}

int mount_context_mount(struct mount_context *ctx, const char *mount_point,
                        int argc, ...) {
    int ret;
    va_list ap;
    va_start(ap, argc);
    ret = mount_context_mountv(ctx, mount_point, argc, ap);
    va_end(ap);
    return ret;
}
