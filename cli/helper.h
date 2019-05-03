#ifndef HELPER_H
#define HELPER_H

#include"tpm-context.h"

struct tpm_args {
    tpm_handle_t handler;
    const gchar *object;
    struct tpm_context *ctx;
};

gboolean tpm_args_init(struct tpm_args *args,
                       const gchar *owner, const gchar *primary,
                       const gchar *private, const gchar *public,
                       const gchar *object);

#define tpm_args_reset(args) (tpm_context_free((args)->ctx))

int _encrypt(const char *in, size_t inlen, char **out, size_t *outlen, void *userdata);
int _decrypt(const char *in, size_t inlen, char **out, size_t *outlen, void *userdata);

#endif /* end of include guard: HELPER_H */
