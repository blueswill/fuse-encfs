#include"tpm-context.h"
#include<tss2/tss2_tcti.h>
#include<tss2/tss2_mu.h>
#include<dlfcn.h>
#include<stdio.h>
#include<gio/gio.h>

#define TPMT_TK_CREATION_EMPTY_INIT { \
    .tag = 0, \
    .hierarchy = 0, \
    .digest = TPM2B_EMPTY_INIT \
}

#define TSS2_RETRY_EXP(expression) \
    ({ \
     TSS2_RC _result = 0; \
     do { \
         _result = (expression); \
     } while ((_result & 0xffff) == TPM2_RC_RETRY); \
     _result; \
     })

#define TPM2B_SENSITIVE_CREATE_EMPTY_INIT { \
    .sensitive = { \
        .data.size = 0, \
        .userAuth.size = 0 \
    } \
}

#define SUPPORTED_ABI_VERSION \
{ \
    .tssCreator = 1, \
    .tssFamily = 2, \
    .tssLevel = 1, \
    .tssVersion = 108, \
}

#define PUBLIC_AREA_TPMA_OBJECT_DEFAULT_INIT { \
    .publicArea = { \
        .type = TPM2_ALG_RSA, \
        .objectAttributes = \
        TPMA_OBJECT_RESTRICTED|TPMA_OBJECT_DECRYPT \
        |TPMA_OBJECT_FIXEDTPM|TPMA_OBJECT_FIXEDPARENT \
        |TPMA_OBJECT_SENSITIVEDATAORIGIN|TPMA_OBJECT_USERWITHAUTH \
        , \
    }, \
}

#define TPMS_AUTH_COMMAND_INIT(handle) { \
    .sessionHandle = (handle), \
    .nonce = TPM2B_EMPTY_INIT, \
    .hmac = TPM2B_EMPTY_INIT, \
    .sessionAttributes = 0 \
}

#define val_check(rval, success_code, err_code) do { \
    if ((rval) != TPM2_RC_SUCCESS) { \
        g_warning("%s return code %X", __func__, (rval)); \
        err_code; \
    } \
    else { \
        success_code; \
    } \
} while (0)

#define return_val_code(rval, code, _default) \
    val_check(rval, return (code), return (_default))
#define return_val_if_fail(rval, code, ret) \
    val_check(rval, ;, code; return ret);
#define return_val(rval) return_val_code(rval, TRUE, FALSE)

struct tpm_context {
    TSS2_SYS_CONTEXT *sapi_ctx;
};

static gboolean tpm2_password(const gchar *password, TPM2B_AUTH *dest) {
    size_t wrote = snprintf((char *)&dest->buffer, BUFFER_SIZE(typeof(*dest), buffer),
                            "%s", password);
    if (wrote >= BUFFER_SIZE(typeof(*dest), buffer)) {
        dest->size = 0;
        return FALSE;
    }
    dest->size = wrote;
    return TRUE;
}

static TSS2_TCTI_CONTEXT *tpm2_tcti_ldr_load(void) {
    const char *ldr_path = "libtss2-tcti-tabrmd.so.0";
    void *handle = dlopen(ldr_path, RTLD_LAZY);
    if (!handle) {
        g_warning(dlerror());
        return NULL;
    }
    TSS2_TCTI_INFO_FUNC infofn = (TSS2_TCTI_INFO_FUNC)dlsym(handle, TSS2_TCTI_INFO_SYMBOL);
    if (!infofn) {
        g_warning("symbol %s not found in library %s", TSS2_TCTI_INFO_SYMBOL, ldr_path);
        dlclose(handle);
        return NULL;
    }
    const TSS2_TCTI_INFO *info = infofn();
    TSS2_TCTI_INIT_FUNC init = info->init;
    size_t size;
    TSS2_RC rc = init(NULL, &size, NULL);
    return_val_if_fail(rc, dlclose(handle), NULL);
    TSS2_TCTI_CONTEXT *ctx = g_malloc0(size);
    rc = init(ctx, &size, NULL);
    return_val_if_fail(rc, { dlclose(handle); g_free(ctx); }, NULL);
    dlclose(handle);
    return ctx;
}

static TSS2_SYS_CONTEXT *sapi_ctx_init(TSS2_TCTI_CONTEXT *tcti_ctx) {
    if (!tcti_ctx)
        return NULL;
    TSS2_ABI_VERSION abi_version = SUPPORTED_ABI_VERSION;
    size_t size = Tss2_Sys_GetContextSize(0);
    TSS2_SYS_CONTEXT *sapi_ctx = g_malloc0(size);
    TSS2_RC rval = Tss2_Sys_Initialize(sapi_ctx, size, tcti_ctx, &abi_version);
    return_val_if_fail(rval, g_free(sapi_ctx), NULL);
    return sapi_ctx;
}

struct tpm_context *tpm_context_new(void) {
    struct tpm_context *ctx = g_new0(struct tpm_context, 1);
    ctx->sapi_ctx = sapi_ctx_init(tpm2_tcti_ldr_load());
    if (!ctx->sapi_ctx) {
        g_free(ctx);
        return NULL;
    }
    return ctx;
}

void tpm_context_free(struct tpm_context *ctx) {
    TSS2_RC rc;
    TSS2_TCTI_CONTEXT *tcti;
    if (!ctx)
        return;
    if (ctx->sapi_ctx) {
        rc = Tss2_Sys_GetTctiContext(ctx->sapi_ctx, &tcti);
        return_val_if_fail(rc, ;, ;);
        Tss2_Tcti_Finalize(tcti);
        g_free(tcti);
        Tss2_Sys_Finalize(ctx->sapi_ctx);
        g_free(ctx->sapi_ctx);
    }
    g_free(ctx);
}

static gboolean change_auth(struct tpm_context *ctx, TPMI_RH_HIERARCHY_AUTH hierarchy,
                            const gchar *new, const gchar *old) {
    TPM2B_AUTH newAuth = {};
    TSS2L_SYS_AUTH_COMMAND sessionsData = {
        .count = 1,
        .auths = { TPMS_AUTH_COMMAND_INIT(TPM2_RS_PW)}
    };
    if (new && !tpm2_password(new, &newAuth))
        return FALSE;
    if (old && !tpm2_password(old, &sessionsData.auths[0].hmac))
        return FALSE;
    UINT32 rval = TSS2_RETRY_EXP(Tss2_Sys_HierarchyChangeAuth(ctx->sapi_ctx,
                                                              hierarchy, &sessionsData, &newAuth, 0));
    return_val(rval);
}

gboolean tpm_context_takeownership(struct tpm_context *ctx,
                                   const struct ownership_password *new, const struct ownership_password *old) {
    gboolean result = TRUE;
    if (new->o || old->o)
        result &= change_auth(ctx, TPM2_RH_OWNER, new->o, old->o);
    if (new->e || old->e)
        result &= change_auth(ctx, TPM2_RH_ENDORSEMENT, new->e, old->e);
    if (new->l || old->l)
        result &= change_auth(ctx, TPM2_RH_LOCKOUT, new->l, old->l);
    return result;
}

gboolean tpm_context_load_primary(struct tpm_context *ctx,
                                  const gchar *primary, const gchar *parent,
                                  TPMI_RH_HIERARCHY hierarchy,
                                  TPM2_HANDLE *out_handle) {
    TPMS_AUTH_COMMAND session_data = TPMS_AUTH_COMMAND_INIT(TPM2_RS_PW);
    TPM2B_SENSITIVE_CREATE inSensitive = TPM2B_SENSITIVE_CREATE_EMPTY_INIT;
    TPM2B_PUBLIC in_public = PUBLIC_AREA_TPMA_OBJECT_DEFAULT_INIT;
    if (!tpm2_password(parent, &session_data.hmac))
        return FALSE;
    if (!tpm2_password(primary, &inSensitive.sensitive.userAuth))
        return FALSE;
    in_public.publicArea.nameAlg = 0xb;
    in_public.publicArea.type = 0x1;
    in_public.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
    in_public.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
    in_public.publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
    in_public.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.rsaDetail.keyBits = 2048;
    in_public.publicArea.parameters.rsaDetail.exponent = 0;
    in_public.publicArea.unique.rsa.size = 0;

    UINT32 rval;
    TSS2L_SYS_AUTH_COMMAND sessionsData;
    TSS2L_SYS_AUTH_RESPONSE sessionsDataOut;

    TPM2B_DATA outsideInfo = TPM2B_EMPTY_INIT;
    TPML_PCR_SELECTION creationPCR;
    TPM2B_NAME name = TPM2B_TYPE_INIT(TPM2B_NAME, name);
    TPM2B_PUBLIC outPublic = TPM2B_EMPTY_INIT;
    TPM2B_CREATION_DATA creationData = TPM2B_EMPTY_INIT;
    TPM2B_DIGEST creationHash = TPM2B_TYPE_INIT(TPM2B_DIGEST, buffer);
    TPMT_TK_CREATION creationTicket = TPMT_TK_CREATION_EMPTY_INIT;
    sessionsData.count = 1;
    sessionsData.auths[0] = session_data;
    inSensitive.size = inSensitive.sensitive.userAuth.size +
        sizeof(inSensitive.size);
    creationPCR.count = 0;
    rval = TSS2_RETRY_EXP(Tss2_Sys_CreatePrimary(ctx->sapi_ctx, hierarchy, &sessionsData,
                                                 &inSensitive, &in_public, &outsideInfo,&creationPCR,
                                                 out_handle, &outPublic, &creationData,
                                                 &creationHash, &creationTicket, &name,
                                                 &sessionsDataOut));
    return_val(rval);
}

gboolean tpm_context_create_rsa(struct tpm_context *ctx, tpm_handle_t *parent_handle,
                                const gchar *parentpass, const gchar *subobjpass,
                                TPM2B_PRIVATE *out_private, TPM2B_PUBLIC *out_public) {
    TSS2_RC rval;
    TSS2L_SYS_AUTH_COMMAND sessionsData;
    TSS2L_SYS_AUTH_RESPONSE sessionsDataOut;
    TPM2B_DATA outsideInfo = TPM2B_EMPTY_INIT;
    TPML_PCR_SELECTION creationPCR;
    TPM2B_CREATION_DATA creationData = TPM2B_EMPTY_INIT;
    TPM2B_DIGEST creationHash = TPM2B_TYPE_INIT(TPM2B_DIGEST, buffer);
    TPMT_TK_CREATION creationTicket = TPMT_TK_CREATION_EMPTY_INIT;

    TPMS_AUTH_COMMAND session_data = TPMS_AUTH_COMMAND_INIT(TPM2_RS_PW);
    TPM2B_SENSITIVE_CREATE inSensitive = TPM2B_SENSITIVE_CREATE_EMPTY_INIT;
    TPM2B_PUBLIC in_public = PUBLIC_AREA_TPMA_OBJECT_DEFAULT_INIT;
    if (!tpm2_password(parentpass, &session_data.hmac))
        return FALSE;
    if (!tpm2_password(subobjpass, &inSensitive.sensitive.userAuth))
        return FALSE;
     in_public.publicArea.type = TPM2_ALG_RSA;
     in_public.publicArea.nameAlg = TPM2_ALG_SHA256;
    /*
     *in_public.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
     *in_public.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
     *in_public.publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
     */
    in_public.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_NULL;
    in_public.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.rsaDetail.keyBits = 2048;
    in_public.publicArea.parameters.rsaDetail.exponent = 0;
    in_public.publicArea.unique.rsa.size = 0;
    in_public.publicArea.objectAttributes = TPMA_OBJECT_DECRYPT|TPMA_OBJECT_SIGN_ENCRYPT|TPMA_OBJECT_FIXEDTPM
                                            |TPMA_OBJECT_FIXEDPARENT|TPMA_OBJECT_SENSITIVEDATAORIGIN
                                            |TPMA_OBJECT_USERWITHAUTH;
    sessionsData.count = 1;
    sessionsData.auths[0] = session_data;
    inSensitive.size = inSensitive.sensitive.userAuth.size + 2;
    creationPCR.count = 0;
    rval = TSS2_RETRY_EXP(Tss2_Sys_Create(ctx->sapi_ctx, *parent_handle, &sessionsData, &inSensitive,
                                          &in_public, &outsideInfo, &creationPCR,
                                          out_private, out_public,
                                          &creationData, &creationHash, &creationTicket,
                                          &sessionsDataOut));
    return_val(rval);
}

gboolean tpm_context_load_rsa(struct tpm_context *ctx, tpm_handle_t *parent_handle,
                              const gchar *parent_pass,
                              TPM2B_PRIVATE *in_private, TPM2B_PUBLIC *in_public,
                              tpm_handle_t *out_handle) {
    TPMS_AUTH_COMMAND session_data = TPMS_AUTH_COMMAND_INIT(TPM2_RS_PW);
    UINT32 rval;
    TSS2L_SYS_AUTH_COMMAND sessionsData;
    TSS2L_SYS_AUTH_RESPONSE sessionsDataOut;
    TPM2B_NAME nameExt = TPM2B_TYPE_INIT(TPM2B_NAME, name);
    if (!tpm2_password(parent_pass, &session_data.hmac))
        return FALSE;
    sessionsData.count = 1;
    sessionsData.auths[0] = session_data;
    rval = TSS2_RETRY_EXP(Tss2_Sys_Load(ctx->sapi_ctx, *parent_handle, &sessionsData,
                                        in_private, in_public, out_handle, &nameExt,
                                        &sessionsDataOut));
    return_val(rval);
}

GBytes *tpm_context_encrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                GBytes *in) {
    TSS2_RC rval;
    TPM2B_PUBLIC_KEY_RSA in_msg = TPM2B_TYPE_INIT(TPM2B_PUBLIC_KEY_RSA, buffer);
    TPM2B_PUBLIC_KEY_RSA out_msg = TPM2B_TYPE_INIT(TPM2B_PUBLIC_KEY_RSA, buffer);
    TPMT_RSA_DECRYPT scheme;
    TPM2B_DATA label;
    TSS2L_SYS_AUTH_RESPONSE out_sessions_data;
    scheme.scheme = TPM2_ALG_RSAES;
    label.size = 0;
    gconstpointer data;
    gsize size;
    data = g_bytes_get_data(in, &size);
    if (!data || size > in_msg.size)
        return NULL;
    in_msg.size = size;
    g_memmove(in_msg.buffer, data, size);
    rval = TSS2_RETRY_EXP(Tss2_Sys_RSA_Encrypt(ctx->sapi_ctx, *handle,
                                               NULL, &in_msg,
                                               &scheme, &label, &out_msg,
                                               &out_sessions_data));
    return_val_code(rval, g_bytes_new(out_msg.buffer, out_msg.size), NULL);
}

GBytes *tpm_context_decrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                const gchar *objpass, GBytes *in){
    TSS2_RC rval;
    TPM2B_PUBLIC_KEY_RSA in_msg = TPM2B_TYPE_INIT(TPM2B_PUBLIC_KEY_RSA, buffer);
    TPM2B_PUBLIC_KEY_RSA out_msg = TPM2B_TYPE_INIT(TPM2B_PUBLIC_KEY_RSA, buffer);
    TSS2L_SYS_AUTH_COMMAND sessions_data = { 1, { TPMS_AUTH_COMMAND_INIT(TPM2_RS_PW) }};
    TPMT_RSA_DECRYPT scheme;
    TPM2B_DATA label;
    TSS2L_SYS_AUTH_RESPONSE out_sessions_data;
    gconstpointer data;
    gsize size;

    scheme.scheme = TPM2_ALG_RSAES;
    label.size = 0;
    if (!tpm2_password(objpass, &sessions_data.auths[0].hmac))
        return NULL;
    data = g_bytes_get_data(in, &size);
    if (!data || size > in_msg.size)
        return NULL;
    in_msg.size = size;
    g_memmove(in_msg.buffer, data, size);
    rval = TSS2_RETRY_EXP(Tss2_Sys_RSA_Decrypt(ctx->sapi_ctx, *handle,
                                               &sessions_data, &in_msg, &scheme,
                                               &label, &out_msg, &out_sessions_data));
    return_val_code(rval, g_bytes_new(out_msg.buffer, out_msg.size), NULL);
}

static gboolean files_load_bytes_from_path(const char *path, UINT8 *buf, UINT16 *size) {
    if (!buf || !size || !path)
        return FALSE;
    g_autofree gchar *tmp = NULL;
    gsize length;
    g_autoptr(GFile) file = g_file_new_for_path(path);
    g_autoptr(GError) err = NULL;
    gboolean ret = g_file_load_contents(file, NULL, &tmp, &length, NULL, &err);
    if (!ret) {
        g_warning(err->message);
        return FALSE;
    }
    if (length > *size)
        return FALSE;
    memmove(buf, tmp, length);
    *size = length;
    return TRUE;
}

static gboolean files_save_bytes_to_file(const char *path, UINT8 *buf, UINT16 length) {
    g_autoptr(GFile) file = g_file_new_for_path(path);
    g_autoptr(GError) err = NULL;
    gboolean ret = g_file_replace_contents(file, (char *)buf, length, NULL, TRUE,
                                           G_FILE_CREATE_REPLACE_DESTINATION, NULL,
                                           NULL, &err);
    if (!ret) {
        g_warning(err->message);
        return FALSE;
    }
    return TRUE;
}

#define SAVE_TYPE(type, name) \
    gboolean tpm_util_save_##name(type *name, const char *path) { \
        size_t offset = 0; \
        UINT8 buffer[sizeof(*name)]; \
        TSS2_RC rc = Tss2_MU_##type##_Marshal(name, buffer, sizeof(buffer), &offset); \
        return_val_code(rc, files_save_bytes_to_file(path, buffer, offset), FALSE); \
    }

#define LOAD_TYPE(type, name) \
    gboolean tpm_util_load_##name(const char *path, type *name) { \
        UINT8 buffer[sizeof(*name)]; \
        UINT16 size = sizeof(buffer); \
        gboolean res = files_load_bytes_from_path(path, buffer, &size); \
        if (!res) \
        return FALSE; \
        size_t offset = 0; \
        TSS2_RC rc = Tss2_MU_##type##_Unmarshal(buffer, size, &offset, name); \
        return_val(rc); \
    }

LOAD_TYPE(TPM2B_PRIVATE, private);
SAVE_TYPE(TPM2B_PRIVATE, private);
LOAD_TYPE(TPM2B_PUBLIC, public);
SAVE_TYPE(TPM2B_PUBLIC, public);
