#include<string.h>
#include<unistd.h>
#include<stdarg.h>
#include<pthread.h>
#include<stdint.h>
#include<sys/ioctl.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<linux/fs.h>
#include<gmodule.h>
#include"mount-context.h"
#include"sm4.h"
#include"encfs-cb.h"
#include"check-context.h"

struct mount_context *mount_context_new(int blkfd, struct crypto *crypto, const char *target) {
    int ret = -1;
    struct mount_context *ctx = g_new(struct mount_context, 1);
    struct check_context *check_ctx = check_context_new(blkfd, crypto);
    if (!check_ctx)
        goto end;
    ctx->blkfd = dup(blkfd);
    ctx->keysize = SM4_XTS_KEY_BYTE_SIZE;
    ctx->key = g_new(uint8_t, ctx->keysize);
    ctx->target = g_strdup(target ? target : "");
    pthread_mutex_init(&ctx->mutex, NULL);
    ctx->start_offset = check_context_do_check(check_ctx);
    if (ctx->start_offset <= 0) {
        g_warning("check device error");
        goto end;
    }
    memmove(ctx->key, check_context_get_password(check_ctx), ctx->keysize);
    ioctl(blkfd, BLKGETSIZE64, &ctx->block_size);
    ret = 0;
end:
    check_context_free(check_ctx);
    if (ret < 0) {
        mount_context_free(ctx);
        ctx = NULL;
    }
    return ctx;
}

void mount_context_free(struct mount_context *ctx) {
    if (ctx) {
        g_free(ctx->key);
        g_free(ctx->target);
        close(ctx->blkfd);
        pthread_mutex_destroy(&ctx->mutex);
        g_free(ctx);
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
    ret = mount_context_mount_raw(ctx, &fuse_args);
    fuse_opt_free_args(&fuse_args);
    g_strfreev(args);
    return ret;
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
