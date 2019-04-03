#ifndef ZZN12_HPP
#define ZZN12_HPP

#include <string>

#ifdef __cplusplus
extern "C" {
#include "miracl.h"
}
#endif

class ZZN12 {
    public:
        ZZN12();
        ZZN12(const ZZN12& var);
        ZZN12& operator=(const ZZN12& var);
        ~ZZN12();

    public:
        std::string toByteArray();

    public:
        ZZN12 mul(ZZN12& var); // return this*var
        ZZN12 conj();
        ZZN12 inverse();
        ZZN12 powq(zzn2& var);
        ZZN12 div(ZZN12& var); // return this/y
        ZZN12 pow(big k); // return this^k
        bool isOrderQ(); 

        // pairing
    public:
        static void q_power_frobenius(ecn2 A, zzn2 F);
        static ZZN12 line(ecn2 A, ecn2 *C, ecn2 *B, zzn2 slope, zzn2 extra, BOOL Doubling, big Qx, big Qy);
        static ZZN12 g(ecn2 *A, ecn2 *B, big Qx, big Qy);
        static bool fast_pairing(ZZN12& ret, ecn2 P, big Qx, big Qy, big x, zzn2 X);
        static bool calcRatePairing(ZZN12& ret, ecn2 P, epoint *Q, big x, zzn2 X);


    private:
        void init();
        void release();
        std::string cout_zzn12_big(big& var);

    private:
        zzn4 a, b, c;
        BOOL unitary; // "unitary property means that fast squaring can be used, and inversions are just conjugates
        BOOL miller;  // "miller" property means that arithmetic on this instance can ignore multiplications
        // or divisions by constants - as instance will eventually be raised to (p-1).
};

#endif
