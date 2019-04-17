#include<getopt.h>
#include<stdio.h>
#include<fcntl.h>
#include<string.h>
#include<errno.h>
#include<gmodule.h>
#include"sm9.h"
#include"sm4.h"
#include"encfs_helper.h"
#include"create-context.h"
#include"config.h"

static struct option opts[] = {
    {"master-pair-file", required_argument, NULL, 'm'},
    {"block-device", required_argument, NULL, 'b'},
    {"id", required_argument, NULL, 't'},
    {"id-directory", required_argument, NULL, 'd'},
    {"generate", no_argument, NULL, 'g'},
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
    fputs("    -h, --help                           show this help message\n", stderr);
}

static struct create_context *check_args(int argc, char **argv, char ***id_list) {
    struct master_key_pair *pair;
    const char *master, *blk, *ids, *id_dir;
    g_autofree gchar *masterfile = NULL;
    int generate = 0;
    opterr = 0;
    int ind;
    while ((ind = getopt_long(argc, argv, "gm:b:t:d:h", opts, NULL)) != -1) {
        switch (ind) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'm':
                master = optarg;
                break;
            case 'b':
                blk = optarg;
                break;
            case 't':
                ids = optarg;
                break;
            case 'd':
                id_dir = optarg;
                break;
            case 'g':
                generate = 1;
                break;
            case '?':
                usage();
                exit(EXIT_FAILURE);
        }
    }
    if (!master || !blk || !ids || !id_dir) {
        usage();
        exit(EXIT_FAILURE);
    }
    int blkfd = open(blk, O_RDWR);
    if (blkfd < 0) {
        fprintf(stderr, "open %s error: %s\n", blk, strerror(errno));
        exit(EXIT_FAILURE);
    }
    int iddirfd = open(id_dir, O_RDONLY);
    if (iddirfd < 0) {
        if (errno == ENOENT) {
            if (g_mkdir_with_parents(id_dir, S_IRWXU | S_IRWXG) < 0) {
                fprintf(stderr, "create %s error: %s\n", id_dir, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        else {
            fprintf(stderr, "open %s error: %s\n", id_dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    masterfile = g_strdup(master);
    if (!g_str_has_suffix(masterfile, MASTER_KEY_PAIR_SUFFIX)) {
        gchar *s = g_strconcat(masterfile, MASTER_KEY_PAIR_SUFFIX, NULL);
        g_free(masterfile);
        masterfile = s;
    }
    int masterfd = open(masterfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (masterfd < 0) {
        fprintf(stderr, "open %s error: %s\n", master, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (generate)
        pair = generate_master_key_pair(TYPE_ENCRYPT);
    else
        pair = master_key_pair_read_file(masterfd);
    if (!pair) {
        fputs("failed to get master key pair\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (generate && master_key_pair_write_file(pair, masterfd) < 0) {
        perror("write master key pair error");
        exit(EXIT_FAILURE);
    }
    struct create_context *args = create_context_new(blkfd, iddirfd, pair, generate);
    if (!args)
        exit(EXIT_FAILURE);
    *id_list = g_strsplit(ids, ",", -1);
    return args;
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
