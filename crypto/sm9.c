#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"parameter.h"
#include"sm9_helper.h"

static int init_flag;

int sm9_init(void) {
    int res;
    if (init_flag)
        return 0;
    res = sm9_parameter_init();
    init_flag = !!res;
    return 0;
}

void sm9_release(void) {
    if (!init_flag)
        return;
    sm9_parameter_release();
    init_flag = 0;
}

int sm9_is_init(void) {
    return init_flag;
}
