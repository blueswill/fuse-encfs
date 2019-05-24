#include<getopt.h>
#include<stdio.h>
#include<fcntl.h>
#include<string.h>
#include<errno.h>
#include<gmodule.h>
#include <unistd.h>
#include"sm9.h"
#include"sm4.h"
#include"encfs_helper.h"
#include"create-context.h"
#include"config.h"
#include"tpm-context.h"
#include"helper.h"

static struct option opts[] = {
    {"master-pair-file", required_argument, NULL, 'm'},
    {"block-device", required_argument, NULL, 'b'},
    {"id", required_argument, NULL, 't'},
    {"id-directory", required_argument, NULL, 'd'},
    {"generate", no_argument, NULL, 'g'},

    {"owner", required_argument, NULL, 'o'},
    {"primary", required_argument, NULL, 'p'},
    {"private", required_argument, NULL, 'r'},
    {"public", required_argument, NULL, 'u'},
    {"object", required_argument, NULL, 's'},

    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
};

static void usage(void) {
    fputs("usage:\n", stderr);
    fputs("    -m, --master-pair-file=FILE          store master pair to FILE\n", stderr);
    fputs("    -b, --block-device=DEVICE            specify target block device\n", stderr);
    fputs("    -t, --id=ID1[,ID2]...                only ID1[,ID2]... can access data in block device\n", stderr);
    fputs("    -d, --id-directory=IDDIR             store ID's private key in IDDIR\n", stderr);
    fputs("    -g, --generate                       generate new master pair\n", stderr);
    fputs("    -o, --owner=OWNER                    set owner password\n", stderr);
    fputs("    -p, --primary=PRIMARY                set primary password\n", stderr);
    fputs("    -r, --private=PRIVATE_FILE           set RSA private key file\n", stderr);
    fputs("    -u, --public=PUBLIC_FILE             set RSA public key file\n", stderr);
    fputs("    -s, --object=OBJECT                  set RSA object password\n", stderr);
    fputs("    -h, --help                           show this help message\n", stderr);
}

static struct create_context *check_args(int argc, char **argv, char ***id_list) {
    struct master_key_pair *pair;
    const char *master = NULL, *blk = NULL, *ids = NULL, *id_dir = NULL;
    const gchar *owner = NULL, *primary = NULL, *private = NULL, *public = NULL;
    const gchar *object = NULL;
    struct tpm_args tpm_args;
    g_autofree gchar *masterfile = NULL;
    int generate = 0;
    opterr = 0;
    int ind;
    while ((ind = getopt_long(argc, argv, "gm:b:t:d:o:p:r:u:s:h", opts, NULL)) != -1) {
        switch (ind) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'o': owner = optarg; break;
            case 'p': primary = optarg; break;
            case 'r': private = optarg; break;
            case 'u': public = optarg; break;
            case 's': object = optarg; break;
            case 'm': master = optarg; break;
            case 'b': blk = optarg; break;
            case 't': ids = optarg; break;
            case 'd': id_dir = optarg; break;
            case 'g': generate = 1; break;
            case '?': usage(); exit(EXIT_FAILURE);
        }
    }
    if (!master || !blk || !ids || !id_dir || !private || !public) {
        usage();
        exit(EXIT_FAILURE);
    }
    if (!tpm_args_init(&tpm_args, owner, primary, private, public, object))
        exit(EXIT_FAILURE);
    int blkfd = open(blk, O_RDWR);
    if (blkfd < 0) {
        fprintf(stderr, "open %s error: %s\n", blk, strerror(errno));
        goto free_tpm;
    }
    int iddirfd = open(id_dir, O_RDONLY);
    if (iddirfd < 0) {
        if (errno == ENOENT) {
            if (g_mkdir_with_parents(id_dir, S_IRWXU | S_IRWXG) < 0) {
                fprintf(stderr, "create %s error: %s\n", id_dir, strerror(errno));
                goto free_tpm;
            }
        }
        else {
            fprintf(stderr, "open %s error: %s\n", id_dir, strerror(errno));
                goto free_tpm;
        }
    }
    masterfile = g_strdup(master);
    if (!g_str_has_suffix(masterfile, MASTER_KEY_PAIR_SUFFIX)) {
        gchar *s = g_strconcat(masterfile, MASTER_KEY_PAIR_SUFFIX, NULL);
        g_free(masterfile);
        masterfile = s;
    }
    int masterfd = open(masterfile, O_RDWR);
    if (masterfd > 0)
        pair = master_key_pair_read_file(masterfd, _decrypt, &tpm_args);
    if (!pair) {
        if (generate) {
            if (masterfd > 0)
                close(masterfd);
            if (unlink(masterfile) < 0 && errno != ENOENT) {
                fprintf(stderr, "delete %s error: %s\n", masterfile, strerror(errno));
                goto free_tpm;
            }
            if ((masterfd = open(masterfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP)) < 0) {
                fprintf(stderr, "create %s error: %s\n", masterfile, strerror(errno));
                goto free_tpm;
            }
            pair = generate_master_key_pair(TYPE_ENCRYPT);
            if (master_key_pair_write_file(pair, masterfd, _encrypt, &tpm_args) < 0) {
                perror("write master key pair error");
                goto free_tpm;
            }
        }
        else if (masterfd < 0) {
            fprintf(stderr, "open %s error: %s\n", masterfile, strerror(errno));
            goto free_tpm;
        }
        else {
            fprintf(stderr, "read master key pair error\n");
            goto free_tpm;
        }
    }
    struct create_context *args = create_context_new(blkfd, iddirfd, pair, generate);
    if (!args)
        goto free_tpm;
    *id_list = g_strsplit(ids, ",", -1);
    //now masterfd must be valid
    close(masterfd);
    return args;
free_tpm:
    if (masterfd > 0)
        close(masterfd);
    tpm_args_reset(&tpm_args);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    sm9_init();
    char **id_list;
    struct create_context *args = check_args(argc, argv, &id_list);
    int ret = create_context_create(args, (void *)id_list);
    create_context_free(args);
    g_strfreev(id_list);
    sm9_release();
    return ret;
}
