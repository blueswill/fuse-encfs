#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include<errno.h>
#include<gmodule.h>
#include"config.h"
#include"encfs-cb.h"
#include"encfs_helper.h"
#include"mount-context.h"
#include"sm9.h"
#include"sm4.h"

void *encfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    sm9_init();
    cfg->kernel_cache = 1;
    return fuse_get_context()->private_data;
}

int encfs_getattr(const char *path, struct stat *st, struct fuse_file_info *info) {
    (void)info;
    struct mount_context *ctx = fuse_get_context()->private_data;
    memset(st, 0, sizeof(*st));
    if (!strcmp(path, "/")) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else if (!strcmp(path + 1, "target")) {
        struct stat blkst;
        if(fstat(ctx->blkfd, &blkst) < 0)
            return -errno;
        st->st_size = ctx->block_size - ctx->start_offset;
        st->st_mode = S_IFREG | 0660;
        st->st_nlink = 1;
    }
    else
        return -ENOENT;
    return 0;
}

int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    if (strcmp(path, "/"))
        return -ENOENT;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "target", NULL, 0, 0);
    return 0;
}

int encfs_open(const char *path, struct fuse_file_info *fi) {
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    return 0;
}

static int read_sector(char *buf, size_t size, off_t offset) {
    struct mount_context *ctx = fuse_get_context()->private_data;
    off_t start = FLOOR2(offset, SM4_BLOCK_BYTE_SHIFT);
    off_t sect_off = FLOOR_OFFSET2(offset, SECTOR_SHIFT);
    off_t end;
    size_t s;
    char *in = NULL, *out = NULL;
    uint32_t tweak[SM4_XTS_IV_BYTE_SIZE >> 2] = {};
    int ret = -1;
    if (sect_off + size > SIZE2(SECTOR_SHIFT))
        size = SIZE2(SECTOR_SHIFT) - sect_off;
    end = CEIL2(offset + size, SM4_BLOCK_BYTE_SHIFT);
    s = end - start;
    in = g_new(char, s);
    out = g_new(char, s);
    if (!in || !out)
        goto end;
    ret = pread(ctx->blkfd, in, s, start);
    if (ret < 0)
        goto end;
    if (ret != (int)s) {
        errno = -EIO;
        goto end;
    }
    tweak[0] = INDEX2(start, SECTOR_SHIFT);
    if (sm4_xts(ctx->key, (void *)in, s, (void *)out,
                (void *)tweak, INDEX2(sect_off, SM4_BLOCK_BYTE_SHIFT), 0) < 0)
        goto end;
    memmove(buf, out + FLOOR_OFFSET2(offset, SM4_BLOCK_BYTE_SHIFT), size);
    ret = size;
end:
    g_free(in);
    g_free(out);
    return ret;
}

static int write_sector(const char *buf, size_t size, off_t offset) {
    struct mount_context *context = fuse_get_context()->private_data;
    char cipher[SIZE2(SECTOR_SHIFT)];
    uint32_t tweak[SM4_XTS_KEY_BYTE_SIZE >> 2] = {};
    off_t off = offset, stop = offset + size;
    const char *from = buf;
    while (off < stop) {
        tweak[0] = INDEX2(off, SECTOR_SHIFT);
        if (sm4_xts(context->key, (void *)from, SIZE2(SECTOR_SHIFT),
                    (void *)cipher, (void *)tweak, 0, 1) < 0)
            return -1;
        ssize_t w = pwrite(context->blkfd, cipher, SIZE2(SECTOR_SHIFT), off);
        if (w < 0)
            return -1;
        if (w != SIZE2(SECTOR_SHIFT)) {
            errno = -EIO;
            return -1;
        }
        off += SIZE2(SECTOR_SHIFT);
        from += SIZE2(SECTOR_SHIFT);
    }
    return 0;
}

static int __encfs_read(char *buf, size_t size, off_t offset) {
    ssize_t len = 0;
    int rd;
    while (len < (ssize_t)size) {
        rd = read_sector(buf + len, size - len, offset + len);
        if (rd < 0)
            return -1;
        len += rd;
    }
    return 0;
}

static int __encfs_write(const char *buf, size_t size, off_t offset) {
    int ret = -1;
    off_t start = FLOOR2(offset, SM4_BLOCK_BYTE_SHIFT);
    off_t stop = CEIL2(offset + size, SM4_BLOCK_BYTE_SHIFT);
    off_t sect_off = FLOOR_OFFSET2(offset, SM4_BLOCK_BYTE_SHIFT);
    size_t left = size;
    char *cipher = g_new(char, stop - start);
    const char *from = buf;
    char *to = cipher;
    if (!cipher)
        return -1;
    while (start < stop) {
        size_t s = left;
        if (sect_off + s > SIZE2(SECTOR_SHIFT))
            s = SIZE2(SECTOR_SHIFT) - sect_off;
        if (s == SIZE2(SECTOR_SHIFT))
            memmove(to, from, s);
        else {
            if (__encfs_read(to, SIZE2(SECTOR_SHIFT), start) < 0)
                goto end;
            memmove(to + sect_off, from, s);
        }
        to += SIZE2(SECTOR_SHIFT);
        from += s;
        sect_off = 0;
        start += SIZE2(SECTOR_SHIFT);
        left -= s;
    }
    ret = 0;
end:
    if (!ret) {
        start = FLOOR2(offset, SM4_BLOCK_BYTE_SHIFT);
        off_t stop = CEIL2(offset + size, SM4_BLOCK_BYTE_SHIFT);
        ret = write_sector(cipher, stop - start, start);
    }
    g_free(cipher);
    return ret;
}

int encfs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret;
    struct mount_context *context = fuse_get_context()->private_data;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    if (offset < 0)
        return -EINVAL;
    offset += context->start_offset;
    if ((size_t)offset >= context->block_size)
        return -EINVAL;
    if (offset + size > context->block_size)
        size = context->block_size - offset;
    pthread_mutex_lock(&context->mutex);
    if (__encfs_read(buf, size, offset) < 0) {
        ret = -errno;
        goto free_lock;
    }
    ret = size;
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret = 0;
    struct mount_context *context = fuse_get_context()->private_data;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    if (offset < 0)
        return -EINVAL;
    offset += context->start_offset;
    if (offset + size > context->block_size)
        return -ENOSPC;
    pthread_mutex_lock(&context->mutex);
    if (__encfs_write(buf, size, offset) < 0) {
        ret = -errno;
        goto free_lock;
    }
    ret = size;
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

void encfs_destroy(void *private_data) {
    mount_context_free(private_data);
    sm9_release();
}
