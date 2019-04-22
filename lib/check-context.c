#include<gmodule.h>
#include<sys/mman.h>
#include<unistd.h>
#include<errno.h>
#include<linux/fs.h>
#include<sys/ioctl.h>
#include"check-context.h"
#include"sm4.h"

struct check_context {
    int blkfd;
    struct crypto *crypto;
};

struct check_context *check_context_new(int blkfd, struct crypto *crypto) {
    struct check_context *ctx = g_new(struct check_context, 1);
    ctx->blkfd = dup(blkfd);
    if (ctx->blkfd < 0) {
        g_warning("dup file description error: %s", g_strerror(errno));
        goto end;
    }
    ctx->crypto = crypto;
    return ctx;
end:
    check_context_free(ctx);
    return NULL;
}

void check_context_free(struct check_context *ctx) {
    if (ctx) {
        if (ctx->blkfd >= 0)
            close(ctx->blkfd);
        g_free(ctx);
    }
}

static int check_header(struct block_header *header,
                        struct crypto *crypto) {
    struct block_header *ptr = header;
    while (IS_HEADER(ptr->fs_flag)) {
        int i;
        for (i = 0; i < ptr->header_number; ++i) {
            uint8_t *ciphertext = ptr->ciphertext[i];
            struct cipher *cipher = ciphertext_read((void *)ciphertext, 160);
            char *out = NULL;
            size_t outlen;
            if (cipher && !sm9_decrypt(crypto->pkey, cipher,
                                       crypto->id, crypto->idlen, 1, 0x100,
                                       &out, &outlen) &&
                outlen == SM4_XTS_KEY_BYTE_SIZE) {
                g_free(out);
                ciphertext_free(cipher);
                return (ptr - header + 1) * sizeof(struct block_header);
            }
            ciphertext_free(cipher);
        }
        if (!IS_HEADER_NEXT(ptr->fs_flag))
            return 0;
        ++ptr;
    }
    return -1;
}

int check_context_do_check(struct check_context *ctx) {
    struct block_header *header = MAP_FAILED;
    uint64_t block_size;
    int ret;
    if (ioctl(ctx->blkfd, BLKGETSIZE64, &block_size) < 0) {
        g_warning("fail to get block size: %s", g_strerror(errno));
        return -1;
    }
    if ((header = mmap(NULL, block_size, PROT_READ, MAP_PRIVATE, ctx->blkfd, 0)) == MAP_FAILED) {
        g_warning("fail to map block device: %s", g_strerror(errno));
        return -1;
    }
    ret = check_header(header, ctx->crypto);
    munmap(header, block_size);
    return ret;
}
