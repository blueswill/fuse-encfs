#ifndef ZZN12_H
#define ZZN12_H

#include<stddef.h>
#include<stdint.h>
#include"miracl.h"

struct zzn12 {
    zzn4 a, b, c;
    int unitary;
    int miller;
};

#define zzn2_size(z) \
    (big_size(z.a) + big_size(z.b))
#define zzn4_size(z) \
    (zzn2_size(z.a) + zzn2_size(z.b))
#define zzn12_size(z) \
    (zzn4_size((z).a) + zzn4_size((z).b) + zzn4_size((z).c))

void zzn12_init(struct zzn12 *);
void zzn12_release(struct zzn12 *);
/* buffer size must be at lease zzn12_size(z) */
size_t zzn12_to_string(struct zzn12 *z, uint8_t *buf, size_t size);

struct zzn12 zzn12_copy(struct zzn12 *);
struct zzn12 zzn12_mul(struct zzn12 *a, struct zzn12 *b);
struct zzn12 zzn12_conj(struct zzn12 *);
struct zzn12 zzn12_inverse(struct zzn12 *);
struct zzn12 zzn12_powq(struct zzn12 *, zzn2 *);
struct zzn12 zzn12_div(struct zzn12 *, struct zzn12 *);
struct zzn12 zzn12_pow(struct zzn12 *, big);
int zzn12_isOrderQ(struct zzn12 *s);
void zzn12_q_power_frobenius(ecn2 A, zzn2 F);
struct zzn12 zzn12_line(ecn2 A, ecn2 *C, ecn2 *B, zzn2 slope, zzn2 extra,
        int Doubling, big Qx, big Qy);
struct zzn12 zzn12_g(ecn2 *A, ecn2 *B, big Qx, big Qy);
int zzn12_fast_pairing(struct zzn12 *ret, ecn2 P, big Qx, big Qy, big x, zzn2 X);
int zzn12_calcRatePairing(struct zzn12 *ret, ecn2 P, epoint *Q, big x, zzn2 X);

#endif
