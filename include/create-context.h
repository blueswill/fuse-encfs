#ifndef CREATE_ARGS_H
#define CREATE_ARGS_H

#include<sys/types.h>
#include"sm9.h"

struct create_context;

struct create_context *create_context_new(int blkfd, int iddirfd,
                                          struct master_key_pair *pair, int regenerate);

int create_context_create(struct create_context *ctx, const char **id_list);

void create_context_free(struct create_context *ctx);

#endif
