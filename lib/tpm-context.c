#include"tpm-context.h"
#include<tss2/tss2_tcti.h>
#include<dlfcn.h>
#include<stdio.h>

#define BUFFER_SIZE(type, field) (sizeof(((type *)NULL)->field))
#define TPM2B_TYPE_INIT(type, field) { .size = BUFFER_SIZE(type, field) }
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

#define TPM2B_EMPTY_INIT { .size = 0 }
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

#define return_val(rval) do { \
    if ((rval) != TPM2_RC_SUCCESS) { \
        g_warning("%s return code %X", __func__, (rval)); \
        return FALSE; \
    } \
    return TRUE; \
} while (0)


struct tpm_context {
    TSS2_SYS_CONTEXT *sapi_ctx;
};

static TSS2_TCTI_CONTEXT *tcti_ctx;

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
    if (tcti_ctx)
        return tcti_ctx;
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
    if (rc != TPM2_RC_SUCCESS) {
        g_warning("tcti init failed for library: %s", ldr_path);
        dlclose(handle);
        return NULL;
    }
    TSS2_TCTI_CONTEXT *ctx = g_malloc0(size);
    rc = init(ctx, &size, NULL);
    if (rc != TPM2_RC_SUCCESS) {
        g_warning("tcti init failed for library: %s", ldr_path);
        dlclose(handle);
        g_free(ctx);
        return NULL;
    }
    dlclose(handle);
    tcti_ctx = ctx;
    return ctx;
}

static TSS2_SYS_CONTEXT *sapi_ctx_init(TSS2_TCTI_CONTEXT *tcti_ctx) {
    if (!tcti_ctx)
        return NULL;
    TSS2_ABI_VERSION abi_version = SUPPORTED_ABI_VERSION;
    size_t size = Tss2_Sys_GetContextSize(0);
    TSS2_SYS_CONTEXT *sapi_ctx = g_malloc0(size);
    TSS2_RC rval = Tss2_Sys_Initialize(sapi_ctx, size, tcti_ctx, &abi_version);
    if (rval != TPM2_RC_SUCCESS) {
        g_warning("error 0x%X", rval);
        g_free(sapi_ctx);
        return NULL;
    }
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

gboolean tpm_context_load_primary(struct tpm_context *ctx,
                                  const gchar *primary, const gchar *parent,
                                  TPMI_RH_HIERARCHY hierarchy,
                                  TPM2_HANDLE *out_handle) {
    TPMS_AUTH_COMMAND session_data = {
        .sessionHandle = TPM2_RS_PW,
        .nonce = TPM2B_EMPTY_INIT,
        .hmac = TPM2B_EMPTY_INIT,
        .sessionAttributes = 0
    };
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

    TPMS_AUTH_COMMAND session_data = {
        .sessionHandle = TPM2_RS_PW,
        .sessionAttributes = 0,
        .hmac.size = 0
    };
    TPM2B_SENSITIVE_CREATE inSensitive = TPM2B_SENSITIVE_CREATE_EMPTY_INIT;
    TPM2B_PUBLIC in_public = PUBLIC_AREA_TPMA_OBJECT_DEFAULT_INIT;
    if (!tpm2_password(parentpass, &session_data.hmac))
        return FALSE;
    if (!tpm2_password(subobjpass, &inSensitive.sensitive.userAuth))
        return FALSE;
    in_public.publicArea.type = TPM2_ALG_RSA;
    in_public.publicArea.nameAlg = TPM2_ALG_SHA256;
    in_public.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
    in_public.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
    in_public.publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
    in_public.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.rsaDetail.keyBits = 2048;
    in_public.publicArea.parameters.rsaDetail.exponent = 0;
    in_public.publicArea.unique.rsa.size = 0;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_USERWITHAUTH;
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
    TPMS_AUTH_COMMAND session_data = {
        .sessionHandle = TPM2_RS_PW,
        .nonce = TPM2B_EMPTY_INIT,
        .hmac = TPM2B_EMPTY_INIT,
        .sessionAttributes = 0
    };
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

gboolean tpm_context_encrypt_rsa(struct tpm_context *ctx, tpm_handle_t *handle,
                                 GBytes *in, GBytes *out) {
}
