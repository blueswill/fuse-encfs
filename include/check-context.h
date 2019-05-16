#ifndef CHECK_CONTEXT_H
#define CHECK_CONTEXT_H

#include<stdint.h>
#include"encfs_helper.h"

struct check_context;
struct check_context *check_context_new(int blkfd, struct crypto *crypto);
void check_context_free(struct check_context *ctx);
int check_context_do_check(struct check_context *ctx);
const char *check_context_get_password(struct check_context *ctx);


#endif
