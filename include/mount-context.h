#ifndef MOUNT_ARGS_H
#define MOUNT_ARGS_H

#include<fuse3/fuse.h>
#include"encfs_helper.h"

struct mount_context {
    unsigned char *key;
    size_t keysize;
    int blkfd;
    uint64_t block_size;
    off_t start_offset;
    char *target;
    pthread_mutex_t mutex;
};


struct mount_context *mount_context_new(int blkfd, struct crypto *crypto, const char *target);
void mount_context_free(struct mount_context *ctx);
struct mount_context *mount_context_copy(struct mount_context *ctx);

int mount_context_mount_raw(struct mount_context *ctx, struct fuse_args *args);

int mount_context_mount(struct mount_context *ctx, const char *mount_point,
                        int argc, ...);

int mount_context_mountv(struct mount_context *ctx, const char *mount_point,
                         int argc, va_list ap);

#endif
