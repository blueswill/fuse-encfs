#ifndef CONTEXT_H
#define CONTEXT_H

#include<pthread.h>
#include<fuse3/fuse.h>

struct encfs_context {
    unsigned char *key;
    size_t keysize;
    int blkfd;
    size_t block_size;
    off_t start_offset;
    pthread_mutex_t mutex;
};

struct encfs_context *encfs_context_init(struct fuse_args *fuse_args);
void encfs_context_free(struct encfs_context *context);

#endif
