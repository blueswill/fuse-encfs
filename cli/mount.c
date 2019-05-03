#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<locale.h>
#include"mount-context.h"
#include"helper.h"

struct user_args {
    const char *block_device;
    const char *pkey_file;
    const char *target;
    const char *owner;
    const char *primary;
    const char *private;
    const char *public;
    const char *object;
    int show_help;
};

void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --block-device=DEVICE            set target block device.\n", stderr);
    fputs("    --pkey=PKEYFILE                  private key for block device.\n", stderr);
    fputs("    --target=NAME                    set a meaningful name.\n", stderr);
    fputs("    --owner=OWNER                    set owner password\n", stderr);
    fputs("    --primary=PRIMARY                set primary password\n", stderr);
    fputs("    --private=PRIVATE_FILE           set RSA private key file\n", stderr);
    fputs("    --public=PUBLIC_FILE             set RSA public key file\n", stderr);
    fputs("    --object=OBJECT                  set RSA object password\n", stderr);
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
    struct tpm_args args = {};
    if (STR_EMPTY(user_args->block_device) ||
        STR_EMPTY(user_args->pkey_file) ||
        user_args->show_help) {
        usage(fuse_args);
        goto end;
    }
    if (!tpm_args_init(&args, user_args->owner, user_args->primary,
                       user_args->private, user_args->public, user_args->object))
        goto end;
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
    if (!(crypto = crypto_read_file(cryptofd, _decrypt, &args))) {
        fputs("read pkey file error\n", stderr);
        goto end;
    }
    ctx = mount_context_new(blkfd, crypto, user_args->target);
    crypto_free(crypto);
end:
    if (blkfd >= 0)
        close(blkfd);
    if (cryptofd >= 0)
        close(cryptofd);
    tpm_args_reset(&args);
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
        OPTION("--target=%s", target),
        OPTION("--owner=%s", owner),
        OPTION("--primary=%s", primary),
        OPTION("--private=%s", private),
        OPTION("--public=%s", public),
        OPTION("--object=%s", object),
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
