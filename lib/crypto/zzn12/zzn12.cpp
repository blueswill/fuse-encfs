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


ZZN12::ZZN12()
{
    init();
}

ZZN12::ZZN12(const ZZN12& var)
{
    init();
    zzn4_copy((zzn4*)&var.a, &a); 
    zzn4_copy((zzn4*)&var.b, &b); 
    zzn4_copy((zzn4*)&var.c, &c);
    miller = var.miller;
    unitary = var.unitary;
}

ZZN12& ZZN12::operator=(const ZZN12& var)
{
    zzn4_copy((zzn4*)&var.a, &a);
    zzn4_copy((zzn4*)&var.b, &b);
    zzn4_copy((zzn4*)&var.c, &c);
    miller = var.miller;
    unitary = var.unitary;
    return *this;
}

ZZN12::~ZZN12()
{
    release();
}


void ZZN12::init()
{
    a.a.a = mirvar(0); a.a.b = mirvar(0);
    a.b.a = mirvar(0); a.b.b = mirvar(0); a.unitary = FALSE;
    b.a.a = mirvar(0); b.a.b = mirvar(0);
    b.b.a = mirvar(0); b.b.b = mirvar(0); b.unitary = FALSE;
    c.a.a = mirvar(0); c.a.b = mirvar(0);
    c.b.a = mirvar(0); c.b.b = mirvar(0); c.unitary = FALSE;
    unitary = FALSE; miller = FALSE;
}

void ZZN12::release()
{
    mirkill(a.a.a); mirkill(a.a.b);
    mirkill(a.b.a); mirkill(a.b.b);
    mirkill(b.a.a); mirkill(b.a.b);
    mirkill(b.b.a); mirkill(b.b.b);
    mirkill(c.a.a); mirkill(c.a.b);
    mirkill(c.b.a); mirkill(c.b.b);
}

std::string ZZN12::toByteArray()
{
    std::string result;
    result.append(cout_zzn12_big(c.b.b));
    result.append(cout_zzn12_big(c.b.a));
    result.append(cout_zzn12_big(c.a.b));
    result.append(cout_zzn12_big(c.a.a));
    result.append(cout_zzn12_big(b.b.b));
    result.append(cout_zzn12_big(b.b.a));
    result.append(cout_zzn12_big(b.a.b));
    result.append(cout_zzn12_big(b.a.a));
    result.append(cout_zzn12_big(a.b.b));
    result.append(cout_zzn12_big(a.b.a));
    result.append(cout_zzn12_big(a.a.b));
    result.append(cout_zzn12_big(a.a.a));
    return result;
}

std::string ZZN12::cout_zzn12_big(big& var)
{
    big tmp = NULL;
    tmp = mirvar(0);
    redc(var, tmp);

    int length = tmp->len * sizeof(tmp->w);
    char *buffer = new char[length];
    int ret = big_to_bytes(length, tmp, buffer, TRUE);
    std::string result(buffer, ret);

    delete[] buffer;
    mirkill(tmp);
    return result;
}

ZZN12 ZZN12::mul(ZZN12& var)
{
    // Karatsuba
    zzn4 Z0, Z1, Z2, Z3, T0, T1;
    ZZN12 ret(*this);
    BOOL zero_c, zero_b;

    init_zzn4(Z0);
    init_zzn4(Z1);
    init_zzn4(Z2);
    init_zzn4(Z3);
    init_zzn4(T0);
    init_zzn4(T1);

    if( zzn4_compare(&a, &var.a) && zzn4_compare(&a, &var.a) && zzn4_compare(&a, &var.a) )
    {
        if( unitary == TRUE )
        {
            zzn4_copy(&a, &Z0); zzn4_mul(&a, &a, &ret.a); zzn4_copy(&ret.a, &Z3); zzn4_add(&ret.a, &ret.a, &ret.a);
            zzn4_add(&ret.a, &Z3, &ret.a); zzn4_conj(&Z0, &Z0); zzn4_add(&Z0, &Z0, &Z0); zzn4_sub(&ret.a, &Z0, &ret.a);
            zzn4_copy(&c, &Z1); zzn4_mul(&Z1, &Z1, &Z1); zzn4_tx(&Z1);
            zzn4_copy(&Z1, &Z3); zzn4_add(&Z1, &Z1, &Z1); zzn4_add(&Z1, &Z3, &Z1);
            zzn4_copy(&b, &Z2); zzn4_mul(&Z2, &Z2, &Z2);
            zzn4_copy(&Z2, &Z3); zzn4_add(&Z2, &Z2, &Z2); zzn4_add(&Z2, &Z3, &Z2);
            zzn4_conj(&b, &ret.b); zzn4_add(&ret.b, &ret.b, &ret.b);
            zzn4_conj(&c, &ret.c); zzn4_add(&ret.c, &ret.c, &ret.c); zzn4_negate(&ret.c, &ret.c);
            zzn4_add(&ret.b, &Z1, &ret.b); zzn4_add(&ret.c, &Z2, &ret.c);
        } else
        {
            if( !miller )
            {// Chung-Hasan SQR2
                zzn4_copy(&a, &Z0); zzn4_mul(&Z0, &Z0, &Z0);
                zzn4_mul(&b, &c, &Z1); zzn4_add(&Z1, &Z1, &Z1);
                zzn4_copy(&c, &Z2); zzn4_mul(&Z2, &Z2, &Z2);
                zzn4_mul(&a, &b, &Z3); zzn4_add(&Z3, &Z3, &Z3);
                zzn4_add(&a, &b, &ret.c); zzn4_add(&ret.c, &c, &ret.c); zzn4_mul(&ret.c, &ret.c, &ret.c);
                zzn4_tx(&Z1); zzn4_add(&Z0, &Z1, &ret.a);
                zzn4_tx(&Z2); zzn4_add(&Z3, &Z2, &ret.b);
                zzn4_add(&Z0, &Z1, &T0); zzn4_add(&T0, &Z2, &T0);
                zzn4_add(&T0, &Z3, &T0); zzn4_sub(&ret.c, &T0, &ret.c);
            } else
            {// Chung-Hasan SQR3 - actually calculate 2x^2 !
                // Slightly dangerous - but works as will be raised to p^{k/2}-1
                // which wipes out the 2.
                zzn4_copy(&a, &Z0); zzn4_mul(&Z0, &Z0, &Z0);// a0^2 = S0
                zzn4_copy(&c, &Z2); zzn4_mul(&Z2, &b, &Z2); zzn4_add(&Z2, &Z2, &Z2); // 2a1.a2 = S3
                zzn4_copy(&c, &Z3); zzn4_mul(&Z3, &Z3, &Z3);; // a2^2 = S4
                zzn4_add(&c, &a, &ret.c); // a0+a2
                zzn4_copy(&b, &Z1); zzn4_add(&Z1, &ret.c, &Z1); zzn4_mul(&Z1, &Z1, &Z1);// (a0+a1+a2)^2 =S1
                zzn4_sub(&ret.c, &b, &ret.c); zzn4_mul(&ret.c, &ret.c, &ret.c);// (a0-a1+a2)^2 =S2
                zzn4_add(&Z2, &Z2, &Z2); zzn4_add(&Z0, &Z0, &Z0); zzn4_add(&Z3, &Z3, &Z3);
                zzn4_sub(&Z1, &ret.c, &T0); zzn4_sub(&T0, &Z2, &T0);
                zzn4_sub(&Z1, &Z0, &T1); zzn4_sub(&T1, &Z3, &T1); zzn4_add(&ret.c, &T1, &ret.c);
                zzn4_tx(&Z3); zzn4_add(&T0, &Z3, &ret.b);
                zzn4_tx(&Z2); zzn4_add(&Z0, &Z2, &ret.a);
            }
        }
    } else
    {
        // Karatsuba
        zero_b = zzn4_iszero(&var.b);
        zero_c = zzn4_iszero(&var.c);
        zzn4_mul(&a, &var.a, &Z0); //9
        if( !zero_b ) zzn4_mul(&b, &var.b, &Z2); //+6
        zzn4_add(&a, &b, &T0);
        zzn4_add(&var.a, &var.b, &T1);
        zzn4_mul(&T0, &T1, &Z1); //+9
        zzn4_sub(&Z1, &Z0, &Z1);
        if( !zero_b ) zzn4_sub(&Z1, &Z2, &Z1);
        zzn4_add(&b, &c, &T0);
        zzn4_add(&var.b, &var.c, &T1);
        zzn4_mul(&T0, &T1, &Z3);//+6
        if( !zero_b ) zzn4_sub(&Z3, &Z2, &Z3);
        zzn4_add(&a, &c, &T0);
        zzn4_add(&var.a, &var.c, &T1);
        zzn4_mul(&T0, &T1, &T0);//+9=39 for "special case"
        if( !zero_b ) zzn4_add(&Z2, &T0, &Z2);
        else zzn4_copy(&T0, &Z2);
        zzn4_sub(&Z2, &Z0, &Z2);
        zzn4_copy(&Z1, &ret.b);
        if( !zero_c )
        { // exploit special form of BN curve line function
            zzn4_mul(&c, &var.c, &T0);
            zzn4_sub(&Z2, &T0, &Z2);
            zzn4_sub(&Z3, &T0, &Z3); zzn4_tx(&T0);
            zzn4_add(&ret.b, &T0, &ret.b);
        }
        zzn4_tx(&Z3);
        zzn4_add(&Z0, &Z3, &ret.a);
        zzn4_copy(&Z2, &ret.c);
        if( !var.unitary ) ret.unitary = FALSE;
    }

    release_zzn4(Z0);
    release_zzn4(Z1);
    release_zzn4(Z2);
    release_zzn4(Z3);
    release_zzn4(T0);
    release_zzn4(T1);

    return ret;
}

ZZN12 ZZN12::conj()
{
    ZZN12 ret;
    zzn4_conj(&a, &ret.a);
    zzn4_conj(&b, &ret.b);
    zzn4_negate(&ret.b, &ret.b);
    zzn4_conj(&c, &ret.c);
    ret.miller = miller;
    ret.unitary = unitary;
    return ret;
}

ZZN12 ZZN12::inverse()
{
    zzn4 tmp1, tmp2;
    ZZN12 ret;
    init_zzn4(tmp1);
    init_zzn4(tmp2);

    if( unitary )
    {
        ret =conj();
        goto END;
    }
    //ret.a=a*a-tx(b*c);
    zzn4_mul(&a, &a, &ret.a);
    zzn4_mul(&b, &c, &ret.b); zzn4_tx(&ret.b);
    zzn4_sub(&ret.a, &ret.b, &ret.a);

    //ret.b=tx(c*c)-a*b;
    zzn4_mul(&c, &c, &ret.c); zzn4_tx(&ret.c);
    zzn4_mul(&a, &b, &ret.b); zzn4_sub(&ret.c, &ret.b, &ret.b);

    //ret.c=b*b-a*c;
    zzn4_mul(&b, &b, &ret.c); zzn4_mul(&a, &c, &tmp1); zzn4_sub(&ret.c, &tmp1, &ret.c);

    //tmp1=tx(b*ret.c)+a*ret.a+tx(c*ret.b);
    zzn4_mul(&b, &ret.c, &tmp1); zzn4_tx(&tmp1);
    zzn4_mul(&a, &ret.a, &tmp2); zzn4_add(&tmp1, &tmp2, &tmp1);
    zzn4_mul(&c, &ret.b, &tmp2); zzn4_tx(&tmp2); zzn4_add(&tmp1, &tmp2, &tmp1);
    zzn4_inv(&tmp1);
    zzn4_mul(&ret.a, &tmp1, &ret.a);
    zzn4_mul(&ret.b, &tmp1, &ret.b);
    zzn4_mul(&ret.c, &tmp1, &ret.c);

END:
    release_zzn4(tmp1);
    release_zzn4(tmp2);
    return ret;
}

ZZN12 ZZN12::powq(zzn2& var)
{
    ZZN12 ret(*this);
    zzn2 X2, X3;

    init_zzn2(X2);
    init_zzn2(X3);

    zzn2_mul(&var, &var, &X2);
    zzn2_mul(&X2, &var, &X3);
    zzn4_powq(&X3, &ret.a); zzn4_powq(&X3, &ret.b); zzn4_powq(&X3, &ret.c);
    zzn4_smul(&ret.b, &sm9_parameter.norm_x, &ret.b);
    zzn4_smul(&ret.c, &X2, &ret.c);

    release_zzn2(X2);
    release_zzn2(X3);
    return ret;
}

ZZN12 ZZN12::div(ZZN12& var)
{
    ZZN12 y = var.inverse();
    return mul(y);
}

ZZN12 ZZN12::pow(big k)
{
    ZZN12 ret;
    big zero, tmp, tmp1;
    int nb, i;
    BOOL invert_it;

    init_big(zero);
    init_big(tmp);
    init_big(tmp1);

    copy(k, tmp1);
    invert_it = FALSE;
    if( mr_compare(tmp1, zero) == 0 )
    {
        tmp = get_mip()->one;
        zzn4_from_big(tmp, &ret.a);
        goto END;
    }
    if( mr_compare(tmp1, zero) < 0 )
    {
        negify(tmp1, tmp1); invert_it = TRUE;
    }
    nb = logb2(k);

    ret = *this;
    if( nb > 1 ) for( i = nb - 2; i >= 0; i-- )
    {
        ret = ret.mul(ret);
        if( mr_testbit(k, i) ) ret = ret.mul(*this);
    }
    if( invert_it ) ret = ret.inverse();

END:
    release_big(zero);
    release_big(tmp);
    release_big(tmp1);
    return ret;
}

bool ZZN12::isOrderQ()
{
    bool result = false;
    ZZN12 v(*this);
    ZZN12 w(*this);
    big six;

    init_big(six);

    convert(6, six);

    w = w.powq(sm9_parameter.norm_x);
    v = v.pow(sm9_parameter.param_t);
    v = v.pow(sm9_parameter.param_t);
    v = v.pow(six);

    if( zzn4_compare(&w.a, &v.a) && zzn4_compare(&w.a, &v.a) && zzn4_compare(&w.a, &v.a) ) {
        result = true;
        goto END;
    }

END:
    release_big(six);

    return result;
}


void ZZN12::q_power_frobenius(ecn2 A, zzn2 F)
{
    // Fast multiplication of A by q (for Trace-Zero group members only)
    zzn2 x, y, z, w, r;

    init_zzn2(x);
    init_zzn2(y);
    init_zzn2(z);
    init_zzn2(w);
    init_zzn2(r);

    ecn2_get(&A, &x, &y, &z);
    zzn2_copy(&F, &r);//r=F
    if( get_mip()->TWIST == MR_SEXTIC_M ) zzn2_inv(&r); // could be precalculated
    zzn2_mul(&r, &r, &w);//w=r*r
    zzn2_conj(&x, &x); zzn2_mul(&w, &x, &x);
    zzn2_conj(&y, &y); zzn2_mul(&w, &r, &w); zzn2_mul(&w, &y, &y);
    zzn2_conj(&z, &z);
    ecn2_setxyz(&x, &y, &z, &A);

    release_zzn2(x);
    release_zzn2(y);
    release_zzn2(z);
    release_zzn2(w);
    release_zzn2(r);
}



ZZN12 ZZN12::line(ecn2 A, ecn2 *C, ecn2 *B, zzn2 slope, zzn2 extra, BOOL Doubling, big Qx, big Qy)
{
    ZZN12 ret;
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
    if( Doubling )
    {
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
        if( get_mip()->TWIST == MR_SEXTIC_M )
        { // "multiplied across" by i to simplify
            zzn2_from_big(Qy, &QY);
            zzn2_txx(&QY);
            zzn2_mul(&U, &QY, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Y, &(ret.c.b));
        }
        if( get_mip()->TWIST == MR_SEXTIC_D )
        {
            zzn2_smul(&U, Qy, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Y, &(ret.b.b));
        }
    } else
    { //slope*X-Y*Z
        ecn2_getxy(B, &X, &Y);
        zzn2_mul(&slope, &X, &X);
        zzn2_mul(&Y, &CZ, &Y);
        zzn2_sub(&X, &Y, &X);
        //(-slope*Qx)
        nres(Qx, QX);
        zzn2_smul(&slope, QX, &Z);
        zzn2_negate(&Z, &Z);
        if( get_mip()->TWIST == MR_SEXTIC_M )
        {
            zzn2_from_big(Qy, &QY);
            zzn2_txx(&QY);
            zzn2_mul(&CZ, &QY, &QY);
            zzn4_from_zzn2s(&QY, &X, &ret.a);
            zzn2_copy(&Z, &(ret.c.b));
        }
        if( get_mip()->TWIST == MR_SEXTIC_D )
        {
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

ZZN12 ZZN12::g(ecn2 *A, ecn2 *B, big Qx, big Qy)
{
    ZZN12 ret;
    zzn2 lam, extra;
    ecn2 P;
    BOOL Doubling;

    init_zzn2(lam);
    init_zzn2(extra);
    init_ecn2(P);

    ecn2_copy(A, &P);
    Doubling = ecn2_add2(B, A, &lam, &extra);
    if( A->marker == MR_EPOINT_INFINITY )
    {
        zzn4_from_int(1, &ret.a);
        ret.miller = FALSE;
        ret.unitary = TRUE;
    } else {
        ret = line(P, A, B, lam, extra, Doubling, Qx, Qy);
    }

    release_zzn2(lam);
    release_zzn2(extra);
    release_ecn2(P);
    return ret;
}

bool ZZN12::fast_pairing(ZZN12& ret, ecn2 P, big Qx, big Qy, big x, zzn2 X)
{
    bool result = false;
    int i, nb;
    big n, zero, negify_x;
    ecn2 A, KA;
    ZZN12 t0, x0, x1, x2, x3, x4, x5, res, tmp;

    init_big(n);
    init_big(zero);
    init_big(negify_x);
    init_ecn2(A);
    init_ecn2(KA);

    premult(x, 6, n); incr(n, 2, n);//n=(6*x+2);
    if( mr_compare(x, zero) < 0 ) //x<0
        negify(n, n); //n=-(6*x+2);
    ecn2_copy(&P, &A);
    nb = logb2(n);
    zzn4_from_int(1, &res.a);
    res.unitary = TRUE; //res=1
    res.miller = TRUE; //Short Miller loop

    for( i = nb - 2; i >= 0; i-- )
    {
        res = res.mul(res);
        tmp = g(&A, &A, Qx, Qy);
        res = res.mul(tmp);
        if( mr_testbit(n, i) ) {
            tmp = g(&A, &P, Qx, Qy);
            res = res.mul(tmp);
        }
    }
    // Combining ideas due to Longa, Aranha et al. and Naehrig
    ecn2_copy(&P, &KA);
    q_power_frobenius(KA, X);
    if( mr_compare(x, zero) < 0 )
    {
        ecn2_negate(&A, &A);
        res = res.conj();
    }
    tmp = g(&A, &KA, Qx, Qy);
    res = res.mul(tmp);

    q_power_frobenius(KA, X);
    ecn2_negate(&KA, &KA);
    tmp = g(&A, &KA, Qx, Qy);
    res = res.mul(tmp);

    if( zzn4_iszero(&res.a) && zzn4_iszero(&res.b) && zzn4_iszero(&res.c) )
        goto END;

    // The final exponentiation
    res = res.conj().div(res);
    res.miller = FALSE; res.unitary = FALSE;

    res = res.powq(X).powq(X).mul(res);
    res.miller = FALSE; res.unitary = TRUE;

    // Newer new idea...
    // See "On the final exponentiation for calculating pairings on ordinary elliptic curves"
    // Michael Scott and Naomi Benger and Manuel Charlemagne and Luis J. Dominguez Perez and Ezekiel J. Kachisa
    t0 = res.powq(X);
    x0 = t0.powq(X);
    x1 = res.mul(t0);

    x0 = x0.mul(x1).powq(X);
    x1 = res.inverse();

    negify(x, negify_x); 
    x4 = res.pow(negify_x);
    x3 = x4.powq(X);
    x2 = x4.pow(negify_x);

    x5 = x2.inverse();
    t0 = x2.pow(negify_x);

    x2 = x2.powq(X);
    x4 = x4.div(x2);
    x2 = x2.powq(X);

    res = t0.powq(X);
    t0 = t0.mul(res);

    t0 = t0.mul(t0).mul(x4).mul(x5);
    res = x3.mul(x5).mul(t0);

    t0 = t0.mul(x2);
    res = res.mul(res).mul(t0);

    res = res.mul(res);
    t0 = res.mul(x1);

    res = res.mul(x0);
    t0 = t0.mul(t0).mul(res);

    ret = t0;
    result = true;

END:
    release_big(n);
    release_big(zero);
    release_big(negify_x);
    release_ecn2(A);
    release_ecn2(KA);
    return result;
}

bool ZZN12::calcRatePairing(ZZN12& ret, ecn2 P, epoint *Q, big x, zzn2 X)
{
    bool result = false;
    big Qx, Qy;

    init_big(Qx);
    init_big(Qy);

    ecn2_norm(&P);
    epoint_get(Q, Qx, Qy);
    result = fast_pairing(ret, P, Qx, Qy, x, X);
    if(result) result = ret.isOrderQ();

    release_big(Qx);
    release_big(Qy);
    return result;
}
