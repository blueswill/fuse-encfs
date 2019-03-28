#include<cstring>
#include"zzn12_wrapper.h"
#include"zzn12.h"

#ifdef __cplusplus
extern "C" {
#endif

struct zzn12 {
    ZZN12 inner;
};

struct zzn12 *zzn12_get(void) {
    struct zzn12 *z = new zzn12;
    return z;
}

int zzn12_calc_rate_pairing(struct zzn12 *z, ecn2 P, epoint *Q, big x, zzn2 X) {
    int ret = ZZN12::calcRatePairing(z->inner, P, Q, x, X);
    return ret;
}

size_t zzn12_size(struct zzn12 *z) {
    std::string str = z->inner.toByteArray();
    return str.size();
}

int zzn12_to_string(struct zzn12 *z, uint8_t *buf, size_t size) {
    std::string str = z->inner.toByteArray();
    memmove(buf, str.data(), str.size());
    return str.size();
}

int zzn12_pow(struct zzn12 *z, big b) {
    z->inner = z->inner.pow(b);
    return 0;
}

void zzn12_free(struct zzn12 *z) {
    delete z;
}

#ifdef __cplusplus
}
#endif
