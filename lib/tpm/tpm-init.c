#include<tss2/tss2_sys.h>
#include<gmodule.h>

TSS2_SYS_CONTEXT *init_sys_context(UINT16 max_command_size,
                                   TSS2_TCTI_CONTEXT *tcti_context,
                                   TSS2_ABI_VERSION *abi_version) {
    UINT16 context_size = Tss2_Sys_GetContextSize(max_command_size);
    TSS2_RC rval;
    TSS2_SYS_CONTEXT *sys_context;

    sys_context = g_malloc(context_size);
    rval = Tss2_Sys_Initialize(sys_context, context_size, tcti_context, abi_version);
    if (rval == TSS2_RC_SUCCESS)
        return sys_context;
    return NULL;
}

void teardown_sys_context(TSS2_SYS_CONTEXT *sys_context) {
    if (sys_context) {
        Tss2_Sys_Finalize(sys_context);
        g_free(sys_context);
    }
}
