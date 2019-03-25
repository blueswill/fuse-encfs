#include "zzn12.h"
#include "parameter.h"
#include"sm9_helper.h"

#define init_zzn4(z) do {\
    init_zzn2((z).a);\
    init_zzn2((z).b);\
    (z).unitary = 0;\
} while (0)

#define release_zzn4(z) do {\
    release_zzn2((z).a);\
    release_zzn2((z).b);\
} while (0)

static inline size_t zzn2_to_string(zzn2 *z, uint8_t *buf, size_t size) {
    size_t s = write_big_buf(z->b, buf, size);
    return s + write_big_buf(z->a, buf + s, size - s);
}
static inline size_t zzn4_to_string(zzn4 *z, uint8_t *buf, size_t size) {
    size_t s = zzn2_to_string(&z->b, buf, size);
    return s + zzn2_to_string(&z->a, buf + s, size - s);
}

size_t zzn12_to_string(struct zzn12 *z, uint8_t *buf, size_t size) {
    size_t s = zzn4_to_string(&z->c, buf, size);
    s += zzn4_to_string(&z->b, buf + s, size - s);
    s += zzn4_to_string(&z->a, buf + s, size - s);
    return s;
}

struct zzn12 zzn12_copy(struct zzn12 *var) {
    struct zzn12 res;
    zzn12_init(&res);
    zzn4_copy(&var->a, &res.a); 
    zzn4_copy(&var->b, &res.b); 
    zzn4_copy(&var->c, &res.c);
    res.miller = var->miller;
    res.unitary = var->unitary;
    return res;
}

void zzn12_init(struct zzn12 *z) {
    init_zzn4(z->a);
    init_zzn4(z->b);
    init_zzn4(z->c);
    z->unitary = 0;
    z->miller = 0;
}

void zzn12_release(struct zzn12 *z) {
    release_zzn4(z->a);
    release_zzn4(z->b);
    release_zzn4(z->c);
}

struct zzn12 zzn12_mul(struct zzn12 *a, struct zzn12 *b) {
    zzn4 Z0, Z1, Z2, Z3, T0, T1;
    struct zzn12 ret = zzn12_copy(a);
    int zero_c, zero_b;

    init_zzn4(Z0);
    init_zzn4(Z1);
    init_zzn4(Z2);
    init_zzn4(Z3);
    init_zzn4(T0);
    init_zzn4(T1);

    if (zzn4_compare(&a->a, &b->a) &&
            zzn4_compare(&a->a, &b->a) &&
            zzn4_compare(&a->a, &b->a)) {
        if (a->unitary) {
            zzn4_copy(&a->a, &Z0);
            zzn4_mul(&a->a, &a->a, &ret.a);
            zzn4_copy(&ret.a, &Z3);
            zzn4_add(&ret.a, &ret.a, &ret.a);
            zzn4_add(&ret.a, &Z3, &ret.a);
            zzn4_conj(&Z0, &Z0);
            zzn4_add(&Z0, &Z0, &Z0);
            zzn4_sub(&ret.a, &Z0, &ret.a);
            zzn4_copy(&a->c, &Z1);
            zzn4_mul(&Z1, &Z1, &Z1);
            zzn4_tx(&Z1);
            zzn4_copy(&Z1, &Z3);
            zzn4_add(&Z1, &Z1, &Z1);
            zzn4_add(&Z1, &Z3, &Z1);
            zzn4_copy(&a->b, &Z2);
            zzn4_mul(&Z2, &Z2, &Z2);
            zzn4_copy(&Z2, &Z3);
            zzn4_add(&Z2, &Z2, &Z2);
            zzn4_add(&Z2, &Z3, &Z2);
            zzn4_conj(&a->b, &ret.b);
            zzn4_add(&ret.b, &ret.b, &ret.b);
            zzn4_conj(&a->c, &ret.c);
            zzn4_add(&ret.c, &ret.c, &ret.c);
            zzn4_negate(&ret.c, &ret.c);
            zzn4_add(&ret.b, &Z1, &ret.b);
            zzn4_add(&ret.c, &Z2, &ret.c);
        }
        else {
            if (!a->miller) {
                zzn4_copy(&a->a, &Z0);
                zzn4_mul(&Z0, &Z0, &Z0);
                zzn4_mul(&a->b, &a->c, &Z1);
                zzn4_add(&Z1, &Z1, &Z1);
                zzn4_copy(&a->c, &Z2);
                zzn4_mul(&Z2, &Z2, &Z2);
                zzn4_mul(&a->a, &a->b, &Z3);
                zzn4_add(&Z3, &Z3, &Z3);
                zzn4_add(&a->a, &a->b, &ret.c);
                zzn4_add(&ret.c, &a->c, &ret.c);
                zzn4_mul(&ret.c, &ret.c, &ret.c);
                zzn4_tx(&Z1);
                zzn4_add(&Z0, &Z1, &ret.a);
                zzn4_tx(&Z2);
                zzn4_add(&Z3, &Z2, &ret.b);
                zzn4_add(&Z0, &Z1, &T0);
                zzn4_add(&T0, &Z2, &T0);
                zzn4_add(&T0, &Z3, &T0);
                zzn4_sub(&ret.c, &T0, &ret.c);
            }
            else {
                zzn4_copy(&a->a, &Z0);
                zzn4_mul(&Z0, &Z0, &Z0);// a0^2 = S0
                zzn4_copy(&a->c, &Z2);
                zzn4_mul(&Z2, &a->b, &Z2);
                zzn4_add(&Z2, &Z2, &Z2);
                // 2a1.a2 = S3
                zzn4_copy(&a->c, &Z3);
                zzn4_mul(&Z3, &Z3, &Z3);;
                // a2^2 = S4
                zzn4_add(&a->c, &a->a, &ret.c);
                // a0+a2
                zzn4_copy(&a->b, &Z1);
                zzn4_add(&Z1, &ret.c, &Z1);
                zzn4_mul(&Z1, &Z1, &Z1);// (a0+a1+a2)^2 =S1
                zzn4_sub(&ret.c, &a->b, &ret.c);
                zzn4_mul(&ret.c, &ret.c, &ret.c);// (a0-a1+a2)^2 =S2
                zzn4_add(&Z2, &Z2, &Z2);
                zzn4_add(&Z0, &Z0, &Z0);
                zzn4_add(&Z3, &Z3, &Z3);
                zzn4_sub(&Z1, &ret.c, &T0);
                zzn4_sub(&T0, &Z2, &T0);
                zzn4_sub(&Z1, &Z0, &T1);
                zzn4_sub(&T1, &Z3, &T1);
                zzn4_add(&ret.c, &T1, &ret.c);
                zzn4_tx(&Z3);
                zzn4_add(&T0, &Z3, &ret.b);
                zzn4_tx(&Z2);
                zzn4_add(&Z0, &Z2, &ret.a);
            }
        }
    }
    else {
        // Karatsuba
        zero_b = zzn4_iszero(&b->b);
        zero_c = zzn4_iszero(&b->c);
        zzn4_mul(&a->a, &b->a, &Z0); //9
        if (!zero_b)
            zzn4_mul(&a->b, &b->b, &Z2); //+6
        zzn4_add(&a->a, &a->b, &T0);
        zzn4_add(&b->a, &b->b, &T1);
        zzn4_mul(&T0, &T1, &Z1); //+9
        zzn4_sub(&Z1, &Z0, &Z1);
        if (!zero_b)
            zzn4_sub(&Z1, &Z2, &Z1);
        zzn4_add(&a->b, &a->c, &T0);
        zzn4_add(&b->b, &b->c, &T1);
        zzn4_mul(&T0, &T1, &Z3);//+6
        if (!zero_b)
            zzn4_sub(&Z3, &Z2, &Z3);
        zzn4_add(&a->a, &a->c, &T0);
        zzn4_add(&b->a, &b->c, &T1);
        zzn4_mul(&T0, &T1, &T0);//+9=39 for "special case"
        if (!zero_b)
            zzn4_add(&Z2, &T0, &Z2);
        else zzn4_copy(&T0, &Z2);
        zzn4_sub(&Z2, &Z0, &Z2);
        zzn4_copy(&Z1, &ret.b);
        if (!zero_c) {
            // exploit special form of BN curve line function
            zzn4_mul(&a->c, &b->c, &T0);
            zzn4_sub(&Z2, &T0, &Z2);
            zzn4_sub(&Z3, &T0, &Z3);
            zzn4_tx(&T0);
            zzn4_add(&ret.b, &T0, &ret.b);
        }
        zzn4_tx(&Z3);
        zzn4_add(&Z0, &Z3, &ret.a);
        zzn4_copy(&Z2, &ret.c);
        if (!b->unitary)
            ret.unitary = 0;
    }

    release_zzn4(Z0);
    release_zzn4(Z1);
    release_zzn4(Z2);
    release_zzn4(Z3);
    release_zzn4(T0);
    release_zzn4(T1);

    return ret;
}

struct zzn12 zzn12_conj(struct zzn12 *s) {
    struct zzn12 ret;
    zzn4_conj(&s->a, &ret.a);
    zzn4_conj(&s->b, &ret.b);
    zzn4_negate(&ret.b, &ret.b);
    zzn4_conj(&s->c, &ret.c);
    ret.miller = s->miller;
    ret.unitary = s->unitary;
    return ret;
}

struct zzn12 zzn12_inverse(struct zzn12 *s) {
    zzn4 tmp1, tmp2;
    struct zzn12 ret;
    init_zzn4(tmp1);
    init_zzn4(tmp2);

    if (s->unitary) {
        ret = zzn12_conj(s);
        goto END;
    }
    //ret.a=a*a-tx(b*c);
    zzn4_mul(&s->a, &s->a, &ret.a);
    zzn4_mul(&s->b, &s->c, &ret.b);
    zzn4_tx(&ret.b);
    zzn4_sub(&ret.a, &ret.b, &ret.a);

    //ret.b=tx(c*c)-a*b;
    zzn4_mul(&s->c, &s->c, &ret.c);
    zzn4_tx(&ret.c);
    zzn4_mul(&s->a, &s->b, &ret.b);
    zzn4_sub(&ret.c, &ret.b, &ret.b);

    //ret.c=b*b-a*c;
    zzn4_mul(&s->b, &s->b, &ret.c);
    zzn4_mul(&s->a, &s->c, &tmp1);
    zzn4_sub(&ret.c, &tmp1, &ret.c);

    //tmp1=tx(b*ret.c)+a*ret.a+tx(c*ret.b);
    zzn4_mul(&s->b, &ret.c, &tmp1);
    zzn4_tx(&tmp1);
    zzn4_mul(&s->a, &ret.a, &tmp2);
    zzn4_add(&tmp1, &tmp2, &tmp1);
    zzn4_mul(&s->c, &ret.b, &tmp2);
    zzn4_tx(&tmp2);
    zzn4_add(&tmp1, &tmp2, &tmp1);
    zzn4_inv(&tmp1);
    zzn4_mul(&ret.a, &tmp1, &ret.a);
    zzn4_mul(&ret.b, &tmp1, &ret.b);
    zzn4_mul(&ret.c, &tmp1, &ret.c);

END:
    release_zzn4(tmp1);
    release_zzn4(tmp2);
    return ret;
}

struct zzn12 zzn12_powq(struct zzn12 *s, zzn2 *var) {
    struct zzn12 ret = zzn12_copy(s);
    zzn2 X2, X3;

    init_zzn2(X2);
    init_zzn2(X3);

    zzn2_mul(var, var, &X2);
    zzn2_mul(&X2, var, &X3);
    zzn4_powq(&X3, &ret.a);
    zzn4_powq(&X3, &ret.b);
    zzn4_powq(&X3, &ret.c);
    zzn4_smul(&ret.b, &sm9_parameter.norm_x, &ret.b);
    zzn4_smul(&ret.c, &X2, &ret.c);

    release_zzn2(X2);
    release_zzn2(X3);
    return ret;
}

struct zzn12 zzn12_div(struct zzn12 *s, struct zzn12 *var) {
    struct zzn12 y = zzn12_inverse(var);
    return zzn12_mul(s, &y);
}

struct zzn12 zzn12_pow(struct zzn12 *s, big k) {
    struct zzn12 ret;
    big zero, tmp, tmp1;
    int nb, i;
    int invert_it;

    init_big(zero);
    init_big(tmp);
    init_big(tmp1);

    copy(k, tmp1);
    invert_it = 0;
    if (mr_compare(tmp1, zero) == 0) {
        tmp = get_mip()->one;
        zzn4_from_big(tmp, &ret.a);
        goto END;
    }
    if (mr_compare(tmp1, zero) < 0) {
        negify(tmp1, tmp1);
        invert_it = 1;
    }
    nb = logb2(k);
    ret = zzn12_copy(s);
    if (nb > 1)
        for (i = nb - 2; i >= 0; i--) {
            ret = zzn12_mul(&ret, &ret);
            if (mr_testbit(k, i))
                ret = zzn12_mul(&ret, s);
        }
    if (invert_it)
        ret = zzn12_inverse(&ret);
END:
    release_big(zero);
    release_big(tmp);
    release_big(tmp1);
    return ret;
}

int zzn12_isOrderQ(struct zzn12 *s) {
    int result = 0;
    struct zzn12 v = zzn12_copy(s);
    struct zzn12 w = zzn12_copy(s);
    big six;

    init_big(six);
    convert(6, six);

    w = zzn12_powq(&w, &sm9_parameter.norm_x);
    v = zzn12_pow(&v, sm9_parameter.param_t);
    v = zzn12_pow(&v, sm9_parameter.param_t);
    v = zzn12_pow(&v, six);

    if (zzn4_compare(&w.a, &v.a) &&
            zzn4_compare(&w.a, &v.a) &&
            zzn4_compare(&w.a, &v.a) ) {
        result = 1;
        goto END;
    }
END:
    release_big(six);
    return result;
}


void zzn12_q_power_frobenius(ecn2 A, zzn2 F) {
    // Fast multiplication of A by q (for Trace-Zero group members only)
    zzn2 x, y, z, w, r;
    init_zzn2(x);
    init_zzn2(y);
    init_zzn2(z);
    init_zzn2(w);
    init_zzn2(r);

    ecn2_get(&A, &x, &y, &z);
    zzn2_copy(&F, &r);//r=F
    if (get_mip()->TWIST == MR_SEXTIC_M)
        zzn2_inv(&r); // could be precalculated
    zzn2_mul(&r, &r, &w);//w=r*r
    zzn2_conj(&x, &x);
    zzn2_mul(&w, &x, &x);
    zzn2_conj(&y, &y);
    zzn2_mul(&w, &r, &w);
    zzn2_mul(&w, &y, &y);
    zzn2_conj(&z, &z);
    ecn2_setxyz(&x, &y, &z, &A);

    release_zzn2(x);
    release_zzn2(y);
    release_zzn2(z);
    release_zzn2(w);
    release_zzn2(r);
}

struct zzn12 zzn12_line(ecn2 A, ecn2 *C, ecn2 *B, zzn2 slope, zzn2 extra,
        int Doubling, big Qx, big Qy) {
    struct zzn12 ret;
    zzn2 X, Y, Z, Z2, U, QY, CZ;
    big QX;

    init_big(QX);
    init_zzn2(X);
    init_zzn2(Y);
    init_zzn2(Z);
    init_zzn2(Z2);
    init_zzn2(U);
    init_zzn2(QY);
    init_zzn2(CZ);

    ecn2_getz(C, &CZ);
    // Thanks to A. Menezes for pointing out this optimization...
    if (Doubling) {
        ecn2_get(&A, &X, &Y, &Z);
        zzn2_mul(&Z, &Z, &Z2); //Z2=Z*Z
        //X=slope*X-extra
        zzn2_mul(&slope, &X, &X);
        zzn2_sub(&X, &extra, &X);
        zzn2_mul(&CZ, &Z2, &U);
        //(-(Z*Z*slope)*Qx);
        nres(Qx, QX);
        zzn2_mul(&Z2, &slope, &Y);
        zzn2_smul(&Y, QX, &Y);
        zzn2_negate(&Y, &Y);
        if (get_mip()->TWIST == MR_SEXTIC_M) {
            // "multiplied across" by i to simplify
            zzn2_from_big(Qy, &QY);
            zzn2_txx(&QY);
            zzn2_mul(&U, &QY, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Y, &(ret.c.b));
        }
        if (get_mip()->TWIST == MR_SEXTIC_D) {
            zzn2_smul(&U, Qy, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Y, &(ret.b.b));
        }
    }
    else {
        //slope*X-Y*Z
        ecn2_getxy(B, &X, &Y);
        zzn2_mul(&slope, &X, &X);
        zzn2_mul(&Y, &CZ, &Y);
        zzn2_sub(&X, &Y, &X);
        //(-slope*Qx)
        nres(Qx, QX);
        zzn2_smul(&slope, QX, &Z);
        zzn2_negate(&Z, &Z);
        if (get_mip()->TWIST == MR_SEXTIC_M) {
            zzn2_from_big(Qy, &QY);
            zzn2_txx(&QY);
            zzn2_mul(&CZ, &QY, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Z, &(ret.c.b));
        }
        if (get_mip()->TWIST == MR_SEXTIC_D) {
            zzn2_smul(&CZ, Qy, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Z, &(ret.b.b));
        }
    }

    release_big(QX);
    release_zzn2(X);
    release_zzn2(Y);
    release_zzn2(Z);
    release_zzn2(Z2);
    release_zzn2(U);
    release_zzn2(QY);
    release_zzn2(CZ);
    return ret;
}

struct zzn12 zzn12_g(ecn2 *A, ecn2 *B, big Qx, big Qy) {
    struct zzn12 ret;
    zzn2 lam, extra;
    ecn2 P;
    int Doubling;

    init_zzn2(lam);
    init_zzn2(extra);
    init_ecn2(P);

    ecn2_copy(A, &P);
    Doubling = ecn2_add2(B, A, &lam, &extra);
    if (A->marker == MR_EPOINT_INFINITY) {
        zzn4_from_int(1, &ret.a);
        ret.miller = FALSE;
        ret.unitary = TRUE;
    }
    else {
        ret = zzn12_line(P, A, B, lam, extra, Doubling, Qx, Qy);
    }

    release_zzn2(lam);
    release_zzn2(extra);
    release_ecn2(P);
    return ret;
}

int zzn12_fast_pairing(struct zzn12 *ret, ecn2 P, big Qx, big Qy, big x, zzn2 X) {
    int result = 0;
    int i, nb;
    big n, zero, negify_x;
    ecn2 A, KA;
    struct zzn12 t0, x0, x1, x2, x3, x4, x5, res, tmp;

    init_big(n);
    init_big(zero);
    init_big(negify_x);
    init_ecn2(A);
    init_ecn2(KA);

    premult(x, 6, n);
    incr(n, 2, n);//n=(6*x+2);
    if (mr_compare(x, zero) < 0) //x<0
        negify(n, n); //n=-(6*x+2);
    ecn2_copy(&P, &A);
    nb = logb2(n);
    zzn4_from_int(1, &res.a);
    res.unitary = 1; //res=1
    res.miller = 1; //Short Miller loop

    for (i = nb - 2; i >= 0; i--) {
        res = zzn12_mul(&res, &res);
        tmp = zzn12_g(&A, &A, Qx, Qy);
        res = zzn12_mul(&res, &tmp);
        if (mr_testbit(n, i)) {
            tmp = zzn12_g(&A, &P, Qx, Qy);
            res = zzn12_mul(&res, &tmp);
        }
    }
    // Combining ideas due to Longa, Aranha et al. and Naehrig
    ecn2_copy(&P, &KA);
    zzn12_q_power_frobenius(KA, X);
    if (mr_compare(x, zero) < 0) {
        ecn2_negate(&A, &A);
        res = zzn12_conj(&res);
    }
    tmp = zzn12_g(&A, &KA, Qx, Qy);
    res = zzn12_mul(&res, &tmp);

    zzn12_q_power_frobenius(KA, X);
    ecn2_negate(&KA, &KA);
    tmp = zzn12_g(&A, &KA, Qx, Qy);
    res = zzn12_mul(&res, &tmp);

    if (zzn4_iszero(&res.a) && zzn4_iszero(&res.b) && zzn4_iszero(&res.c))
        goto END;

    // The final exponentiation
    res = zzn12_conj(&res);
    res = zzn12_div(&res, &res);
    res.miller = 0;
    res.unitary = 0;

    res = zzn12_powq(&res, &X);
    res = zzn12_powq(&res, &X);
    res = zzn12_mul(&res, &res);
    res.miller = 0;
    res.unitary = 0;

    // Newer new idea...
    // See "On the final exponentiation for calculating pairings on ordinary elliptic curves"
    // Michael Scott and Naomi Benger and Manuel Charlemagne and Luis J. Dominguez Perez and Ezekiel J. Kachisa

    t0 = zzn12_powq(&res, &X);
    x0 = zzn12_powq(&t0, &X);
    x1 = zzn12_mul(&res, &t0);

    x0 = zzn12_mul(&x0, &x1);
    x0 = zzn12_powq(&x0, &X);
    x1 = zzn12_inverse(&res);

    negify(x, negify_x); 
    x4 = zzn12_pow(&res, negify_x);
    x3 = zzn12_powq(&x4, &X);
    x2 = zzn12_pow(&x4, negify_x);

    x5 = zzn12_inverse(&x2);
    t0 = zzn12_pow(&x2, negify_x);

    x2 = zzn12_powq(&x2, &X);
    x4 = zzn12_div(&x4, &x2);
    x2 = zzn12_powq(&x2, &X);

    res = zzn12_powq(&t0, &X);
    t0 = zzn12_mul(&t0, &res);

    t0 = zzn12_mul(&t0, &t0);
    t0 = zzn12_mul(&t0, &x4);
    t0 = zzn12_mul(&t0, &x5);
    res = zzn12_mul(&x3, &x5);
    res = zzn12_mul(&res, &t0);

    t0 = zzn12_mul(&t0, &x2);
    res = zzn12_mul(&res, &res);
    res = zzn12_mul(&res, &t0);

    res = zzn12_mul(&res, &res);
    t0 = zzn12_mul(&res, &x1);

    res = zzn12_mul(&res, &x0);
    t0 = zzn12_mul(&t0, &t0);
    t0 = zzn12_mul(&t0, &res);

    *ret = zzn12_copy(&t0);
    result = 1;

END:
    release_big(n);
    release_big(zero);
    release_big(negify_x);
    release_ecn2(A);
    release_ecn2(KA);
    return result;
}

int zzn12_calcRatePairing(struct zzn12 *ret, ecn2 P, epoint *Q, big x, zzn2 X) {
    int result = 0;
    big Qx, Qy;

    init_big(Qx);
    init_big(Qy);

    ecn2_norm(&P);
    epoint_get(Q, Qx, Qy);
    result = zzn12_fast_pairing(ret, P, Qx, Qy, x, X);
    if (result)
        result = zzn12_isOrderQ(ret);

    release_big(Qx);
    release_big(Qy);
    return result;
}


