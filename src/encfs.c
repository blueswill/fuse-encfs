#include<string.h>
#include<unistd.h>
#include<errno.h>
#include"config.h"
#include"encfs.h"
#include"context.h"
#include"encfs_helper.h"

void *encfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
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

static int generate_iv(unsigned char *iv, off_t sector) {
    struct encfs_context *context = fuse_get_context()->private_data;
    unsigned char *ivp = NEW(unsigned char, context->ivsize);
    int len = 0, ret = -EINVAL;
    if (!ivp)
        return -errno;
    memmove(ivp, &sector, sizeof(sector));
    if (EVP_EncryptInit_ex(context->ctx, EVP_aes_256_cbc(), NULL,
                context->key, context->iv) != 1)
        goto free_buf;
    EVP_CIPHER_CTX_set_padding(context->ctx, 0);
    if (EVP_EncryptUpdate(context->ctx, iv, &len, ivp, context->ivsize) != 1)
        goto free_buf;
    if (EVP_EncryptFinal_ex(context->ctx, iv + len, &len) != 1)
        goto free_buf;
    ret = 0;
free_buf:
    free(ivp);
    return ret;
}

static int decrypt_sector(unsigned char *plaintext, const unsigned char *ciphertext,
        off_t sector) {
    struct encfs_context *context = fuse_get_context()->private_data;
    int len, ret;
    unsigned char *iv = NEW(unsigned char, context->ivsize);
    if (!iv || (ret = generate_iv(iv, sector)) < 0) {
        ret = -errno;
        goto free_buf;
    }
    ret = -EINVAL;
    if (EVP_DecryptInit_ex(context->ctx, context->cipher, NULL, context->key, iv) != 1)
        goto free_buf;
    if (EVP_DecryptUpdate(context->ctx, plaintext, &len,
                ciphertext, SECTOR_SIZE) != 1)
        goto free_buf;
    if (EVP_DecryptFinal_ex(context->ctx, plaintext + len, &len) != 1)
        goto free_buf;
    ret = 0;
free_buf:
    free(iv);
    return ret;
}

static int encrypt_sector(unsigned char *ciphertext, const unsigned char *plaintext,
        off_t sector) {
    int len, ret;
    struct encfs_context *context = fuse_get_context()->private_data;
    unsigned char *iv = NEW(unsigned char, context->ivsize);
    if (!iv || (ret = generate_iv(iv, sector)) < 0) {
        ret = -errno;
        goto free_buf;
    }
    ret = -EINVAL;
    if (EVP_EncryptInit_ex(context->ctx, EVP_aes_256_xts(), NULL, context->key, iv) != 1)
        goto free_buf;
    if (EVP_EncryptUpdate(context->ctx, ciphertext, &len, plaintext, SECTOR_SIZE) != 1)
        goto free_buf;
    if (EVP_EncryptFinal_ex(context->ctx, ciphertext + len, &len) != 1)
        goto free_buf;
    ret = 0;
free_buf:
    free(iv);
    return ret;
}

static ssize_t read_sector(unsigned char *buf, off_t offset, size_t size) {
    off_t sector = SECTOR_DOWN(offset);
    off_t sector_offset = SECTOR_OFFSET(offset);
    unsigned char *ciphertext;
    unsigned char *plaintext;
    struct encfs_context *context = fuse_get_context()->private_data;
    int ret;
    if (size + sector_offset > SECTOR_SIZE)
        size = SECTOR_SIZE - sector_offset;
    if (!(ciphertext = NEW(unsigned char, SECTOR_SIZE)) ||
            !(plaintext = NEW(unsigned char, SECTOR_SIZE)))
        return -errno;
    ret = pread(context->blkfd, ciphertext, SECTOR_SIZE, sector);
    if (ret < 0) {
        ret = -errno;
        goto free_buf;
    }
    if (ret != SECTOR_SIZE) {
        ret = -EIO;
        goto free_buf;
    }
    if ((ret = decrypt_sector(plaintext, ciphertext, GET_SECTOR(sector))) < 0)
        goto free_buf;
    memmove(buf, plaintext + sector_offset, size);
    ret = size;
free_buf:
    free(ciphertext);
    free(plaintext);
    return ret;
}

static ssize_t write_sector(unsigned char *cipher, const unsigned char *buf,
        off_t offset, size_t size) {
    unsigned char *sector_data = NEW(unsigned char, SECTOR_SIZE);
    unsigned const char *wait = buf;
    off_t sector = SECTOR_DOWN(offset);
    off_t sector_offset = SECTOR_OFFSET(offset);
    int ret;
    if (!sector_data)
        return -errno;
    if (size + sector_offset > SECTOR_SIZE)
        size = SECTOR_SIZE - sector_offset;
    if (sector_offset != 0 || size != SECTOR_SIZE) {
        if ((ret = read_sector(sector_data, offset, size)) < 0)
            goto free_buf;
        memmove(sector_data + sector_offset, buf + sector_offset, size);
        wait = sector_data;
    }
    if ((ret = encrypt_sector(cipher, wait, GET_SECTOR(sector))) < 0)
        goto free_buf;
free_buf:
    free(sector_data);
    return size;
}

static int __encfs_read(unsigned char *buf, size_t size, off_t offset) {
    size_t len = 0;
    int rd;
    while (len < size) {
        rd = read_sector(buf + len, offset + len, size - len);
        if (rd < 0)
            return rd;
        len += rd;
    }
    return len;
}

static int __encfs_write(const unsigned char *buf, size_t size, off_t offset) {
    int ret;
    size_t len = 0, i = 0;
    size_t alloc = SECTOR_UP(offset + size) - SECTOR_DOWN(offset);
    unsigned char *cipher = NEW(unsigned char, alloc);
    struct encfs_context *context = fuse_get_context()->private_data;
    if (!cipher)
        return -errno;
    while (len < size) {
        ret = write_sector(cipher + i * SECTOR_SIZE, buf + len,
                offset + len, size - len);
        if (ret < 0)
            goto free_buf;
        len += ret;
        ++i;
    }
    ret = pwrite(context->blkfd, cipher, len, SECTOR_DOWN(offset));
    if (ret < 0)
        ret = -errno;
    else if ((size_t)ret != len)
        ret = -EIO;
free_buf:
    free(cipher);
    return ret;
}

int encfs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret = 0;
    struct encfs_context *context = fuse_get_context()->private_data;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    pthread_mutex_lock(&context->mutex);
    offset += context->start_offset;
    if (offset >= context->block_size)
        goto free_lock;
    if (offset + (off_t)size > context->block_size)
        size = context->block_size - offset;
    if ((ret = __encfs_read((unsigned char *)buf, size, offset)) < 0) {
        goto free_lock;
    }
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret = -EIO;
    struct encfs_context *context = fuse_get_context()->private_data;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    pthread_mutex_lock(&context->mutex);
    offset += context->start_offset;
    if (offset + (off_t)size > context->block_size) {
        ret = -ENOSPC;
        goto free_lock;
    }
    if ((ret = __encfs_write((unsigned char *)buf, size, offset)) < 0) {
        goto free_lock;
    }
free_lock:
    pthread_mutex_unlock(&context->mutex);
    return ret;
}

void encfs_destroy(void *private_data) {
    encfs_context_free(private_data);
}


