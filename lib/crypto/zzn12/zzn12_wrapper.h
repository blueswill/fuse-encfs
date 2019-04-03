#ifndef ZZN12_WRAPPER_H
#define ZZN12_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#include"miracl.h"
#include"sm9_helper.h"
#endif

struct zzn12 *zzn12_get(void);

int zzn12_calc_rate_pairing(struct zzn12 *, ecn2 P, epoint *Q, big x, zzn2 X);

int zzn12_to_string(struct zzn12 *, uint8_t *, size_t);

int zzn12_pow(struct zzn12 *, big);

void zzn12_free(struct zzn12 *);

size_t zzn12_size(struct zzn12 *);

int zzn12_to_string(struct zzn12 *, uint8_t *, size_t);

#ifdef __cplusplus
}
#endif

#endif
