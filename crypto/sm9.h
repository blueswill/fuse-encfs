#ifndef SM9_H
#define SM9_H

#include<stdint.h>

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

enum GEN_TYPE {
    TYPE_ENCRYPT
};

const char *sm9_error_string(enum sm9_error e);
int sm9_init(void);
void sm9_release(void);

struct master_key_pair *generate_master_key_pair(enum GEN_TYPE type);

struct private_key *get_private_key(struct master_key_pair *master,
        const char *id, size_t idlen, enum GEN_TYPE type);

#endif
