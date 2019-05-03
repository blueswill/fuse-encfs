#ifndef ENCFS_HELPER_H
#define ENCFS_HELPER_H

#include<stdlib.h>
#include<stdint.h>
#include"sm9.h"

struct crypto {
    struct private_key *pkey;
    char *id;
    size_t idlen;
};

struct block_header {
    uint16_t fs_flag;
    uint8_t header_number;
    uint8_t ciphertext[3][160];
    uint8_t reserve[29];
} __attribute__((packed));

#define FS_FLAG 0x9432
#define FS_TYPE 0xfffe
#define IS_HEADER(flag) (((flag) & FS_TYPE) == FS_FLAG)
#define IS_HEADER_NEXT(flag) ((flag) & ~FS_TYPE)
#define SET_HEADER_FLAG(flag, next) ((flag) = (FS_FLAG | ((next) & ~FS_TYPE)))

#define BIT_MASK(n) ((1 << n) - 1)
#define CEIL_OFFSET2(x, n) (~((x) - 1) & BIT_MASK(n))
#define FLOOR_OFFSET2(x, n) ((x) & BIT_MASK(n))
#define CEIL2(x, n) ((((x) - 1) | BIT_MASK(n)) + 1)
#define FLOOR2(x, n) ((x) & ~BIT_MASK(n))
#define INDEX2(x, n) ((x) >> (n))
#define SIZE2(n) (1 << (n))

typedef int (*ENCRYPT_DECRYPT_FUNC)(const char *in, size_t insize, char **out, size_t *outsize, void *userdata);

struct master_key_pair *master_key_pair_read_file(int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata);
int master_key_pair_write_file(struct master_key_pair *pair, int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata);

struct crypto *crypto_new(struct master_key_pair *pair, const char *id);
struct crypto *crypto_new_private_key(struct private_key *priv, const char *id);
struct crypto *crypto_copy(struct crypto *crypto);
void crypto_free(struct crypto *crypto);

struct crypto *crypto_read_file(int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata);
int crypto_write_file(struct crypto *crypto, int fd, ENCRYPT_DECRYPT_FUNC func, void *userdata);

#endif
