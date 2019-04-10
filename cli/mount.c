#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<locale.h>
#include"mount-context.h"

struct user_args {
    const char *block_device;
    const char *pkey_file;
    int show_help;
};

void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --block-device=[device]  set target block device.\n", stderr);
    fputs("    --pkey=[pkeyfile]        private key for block device.\n", stderr);
    fuse_opt_add_arg(args, "--help");
    fuse_main(args->argc, args->argv, NULL, NULL);
}

#define OPTION(t, p) \
    { t, offsetof(struct user_args, p), 1 }
#define STR_EMPTY(str) (!(str) || *(str) == '\0')

struct mount_context *get_mount_context(struct fuse_args *fuse_args,
                                        struct user_args *user_args) {
    int blkfd = -1, cryptofd = -1;
    struct crypto *crypto = NULL;
    struct mount_context *ctx = NULL;
    if (STR_EMPTY(user_args->block_device) ||
        STR_EMPTY(user_args->pkey_file) ||
        user_args->show_help) {
        usage(fuse_args);
        goto end;
    }
    blkfd = open(user_args->block_device, O_RDWR);
    if (blkfd < 0) {
        fprintf(stderr, "open %s error: %s\n",
                user_args->block_device, strerror(errno));
        goto end;
    }
    cryptofd = open(user_args->pkey_file, O_RDONLY);
    if (cryptofd < 0) {
        fprintf(stderr, "open %s error: %s\n",
                user_args->pkey_file, strerror(errno));
        goto end;
    }
    if (!(crypto = crypto_read_file(cryptofd))) {
        fputs("read pkey file error\n", stderr);
        goto end;
    }
    ctx = mount_context_new(blkfd, crypto);
    crypto_free(crypto);
end:
    if (blkfd >= 0)
        close(blkfd);
    if (cryptofd >= 0)
        close(cryptofd);
    return ctx;
}

int main(int argc, char **argv) {
    int ret = 1;
    struct mount_context *ctx;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct user_args user_args = {};
    const struct fuse_opt option_spec[] = {
        OPTION("--block-device=%s", block_device),
        OPTION("--pkey=%s", pkey_file),
        FUSE_OPT_END
    };
    setlocale(LC_ALL, "");
    if (fuse_opt_parse(&args, &user_args, option_spec, NULL) == -1)
        return -1;
    sm9_init();
    ctx = get_mount_context(&args, &user_args);
    sm9_release();
    if (ctx)
        ret = mount_context_mount_raw(ctx, &args);
    return ret;
}
