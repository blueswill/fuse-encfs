#ifndef CONTEXT_H
#define CONTEXT_H

#include<pthread.h>
#include<fuse3/fuse.h>
#include<openssl/evp.h>

struct encfs_context {
    EVP_CIPHER_CTX *ctx;
    const EVP_CIPHER *cipher;
    unsigned char *key;
    unsigned char *iv;
    pthread_mutex_t mutex;
    int blkfd;
    int ivsize;
    int keysize;
    off_t block_size;
    off_t start_offset;
};

struct encfs_context *encfs_context_init(struct fuse_args *fuse_args);
void encfs_context_free(struct encfs_context *context);

#endif
