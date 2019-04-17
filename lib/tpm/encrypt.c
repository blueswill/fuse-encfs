#include<tss2/tss2_sys.h>
#include<gmodule.h>

UINT32 write_data_string = 0xdeadbeef;

struct SESSION {
    TPMI_DH_OBJECT tpmkey;
    TPMI_DH_ENTITY bind;
    TPM2B_ENCRYPTED_SECRET encrypted_salt;
    TPM2B_MAX_BUFFER salt;
    TPM_SE session_type;
};

void encrypt_decrypt_session(void) {
    TSS2_RC rval = TSS2_RC_SUCCESS;
    SESSION session;
}
