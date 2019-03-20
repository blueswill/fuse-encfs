#include<fuse3/fuse.h>
#include<locale.h>
#include<openssl/err.h>
#include"encfs.h"
#include"context.h"

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct encfs_context *context = encfs_context_init(&args);
    int ret;
    if (!context)
        return EXIT_FAILURE;
    struct fuse_operations oprs = {
        .init = encfs_init,
        .getattr = encfs_getattr,
        .readdir = encfs_readdir,
        .open = encfs_open,
        .read = encfs_read,
        .write = encfs_write,
        .destroy = encfs_destroy
    };
    ret = fuse_main(args.argc, args.argv, &oprs, context);
    fuse_opt_free_args(&args);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
    return ret;
}
