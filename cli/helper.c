#include<gmodule.h>
#include"helper.h"

static int _g_bytes_convert(GBytes *in, char **out, size_t *outlen) {
    if (!in)
        return -1;
    gconstpointer data = g_bytes_get_data(in, outlen);
    if (data) {
        *out = g_malloc(*outlen);
        g_memmove(*out, data, *outlen);
        return 0;
    }
    return -1;
}

int _decrypt(const char *in, size_t inlen, char **out, size_t *outlen, void *userdata) {
    struct tpm_args *args = userdata;
    g_autoptr(GBytes) inbytes = g_bytes_new(in, inlen);
    g_autoptr(GBytes) outbytes = tpm_context_decrypt_rsa(args->ctx, &args->handler, args->object, inbytes);
    return _g_bytes_convert(outbytes, out, outlen);
}

int _encrypt(const char *in, size_t inlen, char **out, size_t *outlen, void *userdata) {
    struct tpm_args *args = userdata;
    g_autoptr(GBytes) inbytes = g_bytes_new(in, inlen);
    g_autoptr(GBytes) outbytes = tpm_context_encrypt_rsa(args->ctx, &args->handler, inbytes);
    return _g_bytes_convert(outbytes, out, outlen);
}

gboolean tpm_args_init(struct tpm_args *args,
                       const gchar *owner, const gchar *primary,
                       const gchar *private, const gchar *public,
                       const gchar *object) {
    args->ctx = tpm_context_new();
    if (!args->ctx)
        return FALSE;
    args->object = object;
    TPM2B_PRIVATE priv = tpm_util_init_private;
    TPM2B_PUBLIC pub = tpm_util_init_public;
    if (!tpm_util_load_private(private, &priv) ||
        !tpm_util_load_public(public, &pub) ||
        !tpm_context_load_primary(args->ctx, primary, owner, TPM2_RH_OWNER, &args->handler) ||
        !tpm_context_load_rsa(args->ctx, &args->handler, primary, &priv, &pub, &args->handler)) {
        if (args->ctx)
            tpm_context_free(args->ctx);
        args->ctx = NULL;
        return FALSE;
    }
    return TRUE;
}


