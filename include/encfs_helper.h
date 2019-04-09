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
    uint8_t header_flag;
    uint8_t header_number;
    uint8_t ciphertext[3][160];
    uint8_t reserve[30];
} __attribute__((packed));

#define HEADER_CUR_TYPE 0x1
#define HEADER_NXT_TYPE 0x2
#define IS_HEADER(flag, type) ((flag) & (type))
#define SET_FLAG(flag, type) ((flag) |= (type))

#define BIT_MASK(n) ((1 << n) - 1)
#define CEIL_OFFSET2(x, n) (~((x) - 1) & BIT_MASK(n))
#define FLOOR_OFFSET2(x, n) ((x) & BIT_MASK(n))
#define CEIL2(x, n) ((((x) - 1) | BIT_MASK(n)) + 1)
#define FLOOR2(x, n) ((x) & ~BIT_MASK(n))
#define INDEX2(x, n) ((x) >> (n))
#define SIZE2(n) (1 << (n))

struct master_key_pair *master_key_pair_read_file(int fd);
int master_key_pair_write_file(struct master_key_pair *pair, int fd);

struct crypto *crypto_new(struct master_key_pair *pair, const char *id);
struct crypto *crypto_new_private_key(struct private_key *priv, const char *id);
void crypto_free(struct crypto *crypto);

struct crypto *crypto_read_file(int fd);
int crypto_write_file(struct crypto *crypto, int fd);

#endif
