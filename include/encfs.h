#ifndef ENCFS_H
#define ENCFS_H

#include<fuse3/fuse.h>

struct crypto_file {
    char priv[129];
    uint8_t idlen;
    uint8_t id[1];
} __attribute__((packed));

struct block_header {
    uint8_t header_flag;
    uint8_t header_number;
    uint8_t ciphertext[3][160];
    uint8_t reserve[30];
} __attribute__((packed));

#define HEADER_CUR_TYPE 0x1
#define HEADER_NXT_TYPE 0x2
#define IS_HEADER(flag, type) ((flag) & (type))
#define SET_FLAG(flag, type) ((flag) |= (type))

void *encfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
int encfs_getattr(const char *path, struct stat *st, struct fuse_file_info *info);
int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags);
int encfs_open(const char *path, struct fuse_file_info *fi);
int encfs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi);
int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi);
void encfs_destroy(void *private_data);

#endif
