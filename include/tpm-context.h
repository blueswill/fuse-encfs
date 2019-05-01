#ifndef TPM_CONTEXT_H
#define TPM_CONTEXT_H

#include<tss2/tss2_sys.h>
#include<gmodule.h>

struct tpm_context;
typedef TPM2_HANDLE tpm_handle_t;

#define BUFFER_SIZE(type, field) (sizeof(((type *)NULL)->field))
#define TPM2B_TYPE_INIT(type, field) { .size = BUFFER_SIZE(type, field) }
#define TPM2B_EMPTY_INIT { .size = 0 }

struct ownership_password {
    const gchar *o, *e, *l;
};

#define ownership_password_init(p, _o, _e, _l) \
    ({ (p)->o = (_o); (p)->e = (_e); (p)->l = (_l); (p); })

struct tpm_context *tpm_context_new(void);
gboolean tpm_context_takeownership(struct tpm_context *ctx,
                                   const struct ownership_password *new, const struct ownership_password *old);
gboolean tpm_context_load_primary(struct tpm_context *ctx,
                                      const gchar *primary, const gchar *pass,
                                      TPMI_RH_HIERARCHY hierarchy,
                                      tpm_handle_t *out_handle);

gboolean tpm_context_create_rsa(struct tpm_context *ctx, tpm_handle_t *parent_handle,
                                const gchar *parent_pass, const gchar *subobjpass,
                                TPM2B_PRIVATE *out_private, TPM2B_PUBLIC *out_public);

gboolean tpm_context_load_rsa(struct tpm_context *ctx, tpm_handle_t *parent_handle,
                              const gchar *parent_pass,
                              TPM2B_PRIVATE *in_private, TPM2B_PUBLIC *in_public,
                              tpm_handle_t *out_handle);

GBytes *tpm_context_encrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                GBytes *in);

GBytes *tpm_context_decrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                const gchar *objpass, GBytes *in);

void tpm_context_free(struct tpm_context *ctx);

#define LOAD_TYPE_DECLARE(type, name) \
    gboolean tpm_util_load_##name(const gchar *file, type *name)
#define SAVE_TYPE_DECLARE(type, name) \
    gboolean tpm_util_save_##name(type *name, const gchar *file)

LOAD_TYPE_DECLARE(TPM2B_PUBLIC, public);
LOAD_TYPE_DECLARE(TPM2B_PRIVATE, private);
SAVE_TYPE_DECLARE(TPM2B_PUBLIC, public);
SAVE_TYPE_DECLARE(TPM2B_PRIVATE, private);

#define tpm_util_init_private TPM2B_TYPE_INIT(TPM2B_PRIVATE, buffer)
#define tpm_util_init_public TPM2B_EMPTY_INIT

#endif
