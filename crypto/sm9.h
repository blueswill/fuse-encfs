#ifndef SM9_H
#define SM9_H

#include<stddef.h>

enum sm9_error {
    SM9_OK = 0x1,
    SM9_ERROR_OTHER,
    SM9_ERROR_NOT_INIT,
    SM9_ERROR_INIT_G1BASEPOINT,
    SM9_ERROR_INIT_G2BASEPOINT,
    SM9_ERROR_CALC_RATE,
    SM9_ERROR_KGC_GENPRIKEY_T1_IS_ZERO,
    SM9_ERROR_KGC_WRONG_PRIKEY_TYPE,
    SM9_ERROR_VERIFY_H_OUTRANGE,
    SM9_ERROR_VERIFY_S_NOT_ON_G1,
    SM9_ERROR_VERIFY_H_VERIFY_FAILED,
    SM9_ERROR_DECAP_C_NOT_ON_G1,
    SM9_ERROR_DECAP_K_IS_ZERO,
    SM9_ERROR_DECRYPT_C1_NOT_ON_G1,
    SM9_ERROR_DECRYPT_K1_IS_ZERO,
    SM9_ERROR_DECRYPT_C3_VERIFY_FAILED,
    SM9_ERROR_KEYEXCHANGE_R_NOT_ON_G1
};

struct master_key_pair;
struct private_key;
struct cipher;

enum GEN_TYPE {
    TYPE_ENCRYPT
};

const char *sm9_error_string(enum sm9_error e);
int sm9_init(void);
int sm9_is_init(void);
void sm9_release(void);

struct master_key_pair *generate_master_key_pair(enum GEN_TYPE type);

struct private_key *get_private_key(struct master_key_pair *master,
        const char *id, size_t idlen, enum GEN_TYPE type);
struct private_key *private_key_read(const char *b, size_t blen);
size_t private_key_size(const struct private_key *);
size_t private_key_write(const struct private_key *, char *b, size_t blen);
void private_key_free(struct private_key *);

struct cipher *sm9_encrypt(
        struct master_key_pair *pair,
        const char *id, size_t idlen,
        const char *data, size_t datalen,
        int isblockcipher, int maclen);

int sm9_decrypt(
        struct private_key *priv,
        const struct cipher *cipher,
        const char *id, size_t idlen,
        int isblockcipher, int maclen,
        char **out, size_t *outlen);

void master_key_pair_free(struct master_key_pair *);

struct cipher *ciphertext_read(const char *in, size_t inlen);
size_t ciphertext_size(const struct cipher *);
size_t ciphertext_write(const struct cipher *, char *out, size_t outlen);
void ciphertext_free(struct cipher *);
#endif
