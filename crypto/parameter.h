#ifndef PARAMETER_H
#define PARAMETER_H

#include"miracl.h"
#include"sm9.h"

#define init_big(x) ((x) = mirvar(0))
#define release_big(x) mirkill(x)
#define init_zzn2(x) (init_big((x).a), init_big((x).b))
#define release_zzn2(x) (release_big((x).a), release_big((x).b))
#define init_epoint(e) ((e) = epoint_init())
#define release_epoint(e) epoint_free(e)
#define init_ecn2(e) do {\
    init_zzn2((e).x);\
    init_zzn2((e).y);\
    init_zzn2((e).z);\
    (e).marker = MR_EPOINT_INFINITY;\
} while (0)
#define release_ecn2(e) do {\
    release_zzn2((e).x);\
    release_zzn2((e).y);\
    release_zzn2((e).z);\
} while (0)

#define read_big(r, b, blen) bytes_to_big(blen, (const char *)(b), r);

struct sm9_parameter {
    enum sm9_error err;
    big param_a, param_b, param_q, param_t, param_N;
    epoint *param_p1;
    ecn2 param_p2;
    zzn2 norm_x;
    miracl *mip;
};

extern struct sm9_parameter sm9_parameter;

int sm9_parameter_init(void);
int sm9_parameter_release(void);

void report_error(enum sm9_error err);
#endif
