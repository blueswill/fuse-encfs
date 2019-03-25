#ifndef PARAMETER_H
#define PARAMETER_H

#include"miracl.h"
#include"sm9.h"

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

#endif
