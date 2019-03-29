#include<sys/random.h>
#include"parameter.h"
#include"sm9_helper.h"

#define BIG_LEN 2000

static unsigned char sm9_q[] = {
    0xB6, 0x40, 0x00, 0x00, 0x02, 0xA3, 0xA6, 0xF1, 0xD6, 0x03, 0xAB, 0x4F, 0xF5, 0x8E, 0xC7, 0x45,
    0x21, 0xF2, 0x93, 0x4B, 0x1A, 0x7A, 0xEE, 0xDB, 0xE5, 0x6F, 0x9B, 0x27, 0xE3, 0x51, 0x45, 0x7D
};

static unsigned char sm9_N[] = {
    0xB6, 0x40, 0x00, 0x00, 0x02, 0xA3, 0xA6, 0xF1, 0xD6, 0x03, 0xAB, 0x4F, 0xF5, 0x8E, 0xC7, 0x44,
    0x49, 0xF2, 0x93, 0x4B, 0x18, 0xEA, 0x8B, 0xEE, 0xE5, 0x6E, 0xE1, 0x9C, 0xD6, 0x9E, 0xCF, 0x25
};

static unsigned char sm9_p1x[] = {
    0x93, 0xDE, 0x05, 0x1D, 0x62, 0xBF, 0x71, 0x8F, 0xF5, 0xED, 0x07, 0x04, 0x48, 0x7D, 0x01, 0xD6,
    0xE1, 0xE4, 0x08, 0x69, 0x09, 0xDC, 0x32, 0x80, 0xE8, 0xC4, 0xE4, 0x81, 0x7C, 0x66, 0xDD, 0xDD
};

static unsigned char sm9_p1y[] = {
    0x21, 0xFE, 0x8D, 0xDA, 0x4F, 0x21, 0xE6, 0x07, 0x63, 0x10, 0x65, 0x12, 0x5C, 0x39, 0x5B, 0xBC,
    0x1C, 0x1C, 0x00, 0xCB, 0xFA, 0x60, 0x24, 0x35, 0x0C, 0x46, 0x4C, 0xD7, 0x0A, 0x3E, 0xA6, 0x16
};

static unsigned char sm9_p2[] = {
    0x85, 0xAE, 0xF3, 0xD0, 0x78, 0x64, 0x0C, 0x98, 0x59, 0x7B, 0x60, 0x27, 0xB4, 0x41, 0xA0, 0x1F,
    0xF1, 0xDD, 0x2C, 0x19, 0x0F, 0x5E, 0x93, 0xC4, 0x54, 0x80, 0x6C, 0x11, 0xD8, 0x80, 0x61, 0x41,
    0x37, 0x22, 0x75, 0x52, 0x92, 0x13, 0x0B, 0x08, 0xD2, 0xAA, 0xB9, 0x7F, 0xD3, 0x4E, 0xC1, 0x20,
    0xEE, 0x26, 0x59, 0x48, 0xD1, 0x9C, 0x17, 0xAB, 0xF9, 0xB7, 0x21, 0x3B, 0xAF, 0x82, 0xD6, 0x5B,
    0x17, 0x50, 0x9B, 0x09, 0x2E, 0x84, 0x5C, 0x12, 0x66, 0xBA, 0x0D, 0x26, 0x2C, 0xBE, 0xE6, 0xED,
    0x07, 0x36, 0xA9, 0x6F, 0xA3, 0x47, 0xC8, 0xBD, 0x85, 0x6D, 0xC7, 0x6B, 0x84, 0xEB, 0xEB, 0x96,
    0xA7, 0xCF, 0x28, 0xD5, 0x19, 0xBE, 0x3D, 0xA6, 0x5F, 0x31, 0x70, 0x15, 0x3D, 0x27, 0x8F, 0xF2,
    0x47, 0xEF, 0xBA, 0x98, 0xA7, 0x1A, 0x08, 0x11, 0x62, 0x15, 0xBB, 0xA5, 0xC9, 0x99, 0xA7, 0xC7
};

static unsigned char sm9_t[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x58, 0xF9, 0x8A
};

static unsigned char sm9_a[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char sm9_b[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05
};

struct sm9_parameter sm9_parameter;

void zzn2_pow(zzn2 *x, big k, zzn2 *r) {
    int i, j, nb, n, nbw, nzs;
    big zero;
    zzn2 u2, t[16];

    init_big(zero);
    init_zzn2(u2);	
    for( i = 0; i < 16; i++ ) {
        init_zzn2(t[i]);
    }

    if( zzn2_iszero(x) ) {
        zzn2_zero(r);
        goto END;
    }
    if( size(k) == 0 )
    {
        zzn2_from_int(1, r);
        goto END;
    }
    if( size(k) == 1 ) {
        zzn2_copy(x, r);
        goto END;
    }

    // Prepare table for windowing
    zzn2_mul(x, x, &u2);
    zzn2_copy(x, &t[0]);
    for( i = 1; i < 16; i++ )
    {
        zzn2_mul(&t[i - 1], &u2, &t[i]);
    }
    // Left to right method - with windows
    zzn2_copy(x, r);
    nb = logb2(k);
    if( nb > 1 ) for( i = nb - 2; i >= 0;)
    {
        //Note new parameter of window_size=5. Default to 5, but reduce to 4 (or even 3) to save RAM
        n = mr_window(k, i, &nbw, &nzs, 5);
        for( j = 0; j < nbw; j++ ) zzn2_mul(r, r, r);
        if( n > 0 ) zzn2_mul(r, &t[n / 2], r);
        i -= nbw;
        if( nzs )
        {
            for( j = 0; j < nzs; j++ ) zzn2_mul(r, r, r);
            i -= nzs;
        }
    }

END:
    release_big(zero);
    release_zzn2(u2);
    for( i = 0; i < 16; i++ ) {
        release_zzn2(t[i]);
    }
}

static void set_frobenius_norm_constant() {
    big p, zero, one, two;
    zzn2 tmp_norm_X;

    init_big(p);
    init_big(zero);
    init_big(one);
    init_big(two);
    init_zzn2(tmp_norm_X);

    convert(0, zero);
    convert(1, one);
    convert(2, two);
    copy(sm9_parameter.mip->modulus, p);
    switch (get_mip()->pmod8) {
        case 5:
            zzn2_from_bigs(zero, one, &tmp_norm_X);// = (sqrt(-2)^(p-1)/2
            break;
        case 3:
            zzn2_from_bigs(one, one, &tmp_norm_X); // = (1+sqrt(-1))^(p-1)/2
            break;
        case 7:
            zzn2_from_bigs(two, one, &tmp_norm_X);// = (2+sqrt(-1))^(p-1)/2
        default:
            break;
    }
    decr(p, 1, p);
    subdiv(p, 6, p);
    zzn2_pow(&tmp_norm_X, p, &sm9_parameter.norm_x);

    release_big(p);
    release_big(zero);
    release_big(one);
    release_big(two);
    release_zzn2(tmp_norm_X);
}

int sm9_parameter_release(void) {
    release_big(sm9_parameter.param_N);
    release_big(sm9_parameter.param_a);
    release_big(sm9_parameter.param_b);
    release_big(sm9_parameter.param_q);
    release_big(sm9_parameter.param_t);
    release_epoint(sm9_parameter.param_p1);
    release_ecn2(sm9_parameter.param_p2);
    release_zzn2(sm9_parameter.norm_x);
    mirexit();
    sm9_parameter.err = SM9_OK;
    return 0;
}

int sm9_parameter_init(void) {
    int result = 0;
    big P1_x = NULL;
    big P1_y = NULL; 
    unsigned int seed;

    sm9_parameter.mip = mirsys(BIG_LEN, 16);
    sm9_parameter.mip->IOBASE = 16;
    sm9_parameter.mip->TWIST = MR_SEXTIC_M;

    getrandom(&seed, sizeof(seed), 0);
    irand(seed);

    init_big(P1_x); 
    init_big(P1_y);

    init_big(sm9_parameter.param_N);
    init_big(sm9_parameter.param_a);
    init_big(sm9_parameter.param_b);
    init_big(sm9_parameter.param_q);
    init_big(sm9_parameter.param_t);
    init_epoint(sm9_parameter.param_p1);
    init_ecn2(sm9_parameter.param_p2);
    init_zzn2(sm9_parameter.norm_x);

    read_big(sm9_parameter.param_N, sm9_N, sizeof(sm9_N));
    read_big(sm9_parameter.param_a, sm9_a, sizeof(sm9_a));
    read_big(sm9_parameter.param_b, sm9_b, sizeof(sm9_b));
    read_big(sm9_parameter.param_q, sm9_q, sizeof(sm9_q));
    read_big(sm9_parameter.param_t, sm9_t, sizeof(sm9_t));

    //Initialize GF(q) elliptic curve, MR_PROJECTIVE specifying projective coordinates
    ecurve_init(sm9_parameter.param_a, sm9_parameter.param_b,
            sm9_parameter.param_q, MR_PROJECTIVE); 

    read_big(P1_x, (char*)sm9_p1x, sizeof(sm9_p1x));
    read_big(P1_y, (char*)sm9_p1y, sizeof(sm9_p1y));

    result = epoint_set(P1_x, P1_y, 0, sm9_parameter.param_p1);
    if (result) {
        result = read_ecn2_byte128(&sm9_parameter.param_p2, sm9_p2);
        if (!result) {
            sm9_parameter.err = SM9_ERROR_INIT_G2BASEPOINT;
            goto end;
        }
    }
    else {
        sm9_parameter.err = SM9_ERROR_INIT_G1BASEPOINT;
        goto end;
    }
    set_frobenius_norm_constant();
    result = 1;
end:
    release_big(P1_x);
    release_big(P1_y);
    if (!result) {
        sm9_parameter_release();
    }
    return result;
}

int is_point_on_g1(epoint *p) {
    int result = 0;
    big x = NULL;
    big y = NULL;
    big x_3 = NULL;
    big tmp = NULL;
    epoint* buf = NULL;

    init_big(x);
    init_big(y);
    init_big(x_3);
    init_big(tmp);
    init_epoint(buf);

    //check y^2=x^3+b
    epoint_get(p, x, y);
    power(x, 3, sm9_parameter.param_q, x_3); //x_3=x^3 mod p
    multiply(x, sm9_parameter.param_a, x);
    divide(x, sm9_parameter.param_q, tmp);
    add(x_3, x, x); //x=x^3+ax+b
    add(x, sm9_parameter.param_b, x);
    divide(x, sm9_parameter.param_q, tmp); //x=x^3+ax+b mod p
    power(y, 2, sm9_parameter.param_q, y); //y=y^2 mod p
    if( mr_compare(x, y) != 0 )
        return 1;

    //check infinity
    ecurve_mult(sm9_parameter.param_N, p, buf);
    result = point_at_infinity(buf);
    release_big(x);
    release_big(y);
    release_big(x_3);
    release_big(tmp);
    release_epoint(buf);
    return result;
}
