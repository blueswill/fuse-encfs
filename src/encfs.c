#include<string.h>
#include<unistd.h>
#include<errno.h>
#include"config.h"
#include"encfs.h"
#include"context.h"
#include"encfs_helper.h"
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
    struct encfs_context *context = fuse_get_context()->private_data;
    memset(st, 0, sizeof(*st));
    if (!strcmp(path, "/")) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else if (!strcmp(path + 1, "target")) {
        struct stat blkst;
        if(fstat(context->blkfd, &blkst) < 0)
            return -errno;
        memmove(st, &blkst, sizeof(*st));
        st->st_size -= context->start_offset;
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

static int read_sector(char *buf, off_t offset, size_t size) {
    struct encfs_context *context = fuse_get_context()->private_data;
    off_t start = FLOOR2(offset, SM4_BLOCK_BYTE_SHIFT);
    off_t sect_off = FLOOR_OFFSET2(offset, SECTOR_SHIFT);
    off_t end;
    size_t s;
    char *in = NULL, *out = NULL;
    uint32_t tweak[SM4_XTS_IV_BYTE_SIZE >> 2];
    int ret = -1;
    if (sect_off + size > SIZE2(SECTOR_SHIFT))
        size = SIZE2(SECTOR_SHIFT) - sect_off;
    end = CEIL2(offset + size, SM4_BLOCK_BYTE_SHIFT);
    s = end - start;
    in = NEW(char, s);
    out = NEW(char, s);
    if (!in || !out)
        goto end;
    ret = pread(context->blkfd, in, s, start);
    if (ret < 0)
        goto end;
    if (ret != (int)s) {
        errno = -EIO;
        goto end;
    }
    tweak[0] = INDEX2(start, SECTOR_SHIFT);
    if (sm4_xts(context->key, (void *)in, s, (void *)out,
                (void *)tweak, INDEX2(sect_off, SM4_BLOCK_BYTE_SHIFT), 0) < 0)
        goto end;
    memmove(buf, in + FLOOR_OFFSET2(offset, SM4_BLOCK_BYTE_SHIFT), size);
    ret = 0;
end:
    free(in);
    free(out);
    return ret;
}

static int write_sector(const char *buf, off_t offset, size_t size) {
    char *cipher = NEW(char, size);
    struct encfs_context *context = fuse_get_context()->private_data;
    uint32_t tweak[SM4_XTS_IV_BYTE_SIZE >> 2];
    size_t off = FLOOR_OFFSET2(offset, SECTOR_SHIFT);
    ssize_t w = -1;
    tweak[0] = INDEX2(offset, SECTOR_SHIFT);
    if (!cipher)
        return -1;
    if (sm4_xts(context->key, (void *)buf, size, (void *)cipher,
                (void *)tweak, INDEX2(off, SM4_BLOCK_BYTE_SHIFT), 1) < 0)
        goto end;
    w = pwrite(context->blkfd, cipher, size, offset);
    if (w < 0)
        goto end;
    if (w != (ssize_t)size) {
        errno = -EIO;
        goto end;
    }
end:
    free(cipher);
    if (w != (ssize_t)size)
        return -1;
    return 0;
}

static int __encfs_read(char *buf, size_t size, off_t offset) {
    ssize_t len = 0;
    int rd;
    while (len < (ssize_t)size) {
        rd = read_sector(buf + len, offset + len, size - len);
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
    size_t s = (size_t)(stop - start);
    char *cipher;
    if (start == offset && s == size) {
        if (write_sector(buf, offset, size) < 0)
            return -1;
        return 0;
    }
    if (!(cipher = NEW(char, s)))
        return -1;
    if (__encfs_read(cipher, s, start) < 0)
        goto free_buf;
    memmove(cipher + FLOOR_OFFSET2(offset, SM4_BLOCK_BYTE_SHIFT),
            buf, size);
    if (write_sector(cipher, start, s) < 0)
        goto free_buf;
    ret = 0;
free_buf:
    free(cipher);
    return ret;
}

int encfs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret;
    struct encfs_context *context = fuse_get_context()->private_data;
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
    ret = 0;
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret = 0;
    struct encfs_context *context = fuse_get_context()->private_data;
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
    ret = 0;
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

void encfs_destroy(void *private_data) {
    encfs_context_free(private_data);
    sm9_release();
}
