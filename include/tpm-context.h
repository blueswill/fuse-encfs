#ifndef TPM_CONTEXT_H
#define TPM_CONTEXT_H

#include<tss2/tss2_sys.h>
#include<gmodule.h>

struct tpm_context;
typedef TPM2_HANDLE tpm_handle_t;

struct tpm_context *tpm_context_new(void);
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

gboolean tpm_context_encrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                 GBytes *in, GBytes *out);

gboolean tpm_context_decrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                 GBytes *in, GBytes *out);
#endif
