#include<getopt.h>
#include<assert.h>
#include<sys/random.h>
#include<sys/mman.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<dirent.h>
#include<unistd.h>
#include"sm9.h"
#include"sm4.h"
#include"encfs.h"
#include"encfs_helper.h"
#include"config.h"

static struct option opts[] = {
    {"master-pair-file", required_argument, NULL, 'm'},
    {"block-device", required_argument, NULL, 'b'},
    {"id", required_argument, NULL, 't'},
    {"id-file", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
};

void usage(void) {
    fputs("usage:\n", stderr);
    fputs("    -m, --master-pair-file=FILE          store master pair to FILE\n", stderr);
    fputs("    -b, --block-device=DEVICE            specify target block device\n", stderr);
    fputs("    -t, --id=ID1[,ID2]...                only ID1[,ID2]... can access data in block device\n", stderr);
    fputs("    -d, --id-directory=IDDIR             store ID's private key in IDDIR\n", stderr);
    fputs("    -h, --help                           show this help message\n", stderr);
}

struct create_args {
    const char *masterfile;
    const char *blk;
    const char *ids;
    const char *iddir;
    int blkfd;
    int iddirfd;
    int regenerate;
    off_t blksize;
    struct master_key_pair *pair;
};

static int id_exists_num;
static char id_exists[256][20];

static void check_ids(struct create_args *args) {
    DIR *dir;
    args->iddirfd = open(args->iddir, O_RDONLY | O_DIRECTORY);
    if (args->iddirfd < 0 || !(dir = fdopendir(dup(args->iddirfd)))) {
        fprintf(stderr, "open %s error: %s", args->iddir, strerror(errno));
        exit(EXIT_FAILURE);
    }
    for (struct dirent *e = readdir(dir); e; e = readdir(dir)) {
        size_t namelen = strlen(e->d_name);
        if (e->d_type == DT_REG && id_exists_num < 256)
            memmove(id_exists[id_exists_num++], e->d_name,
                    (namelen < 19 ? namelen : 19));
    }
    closedir(dir);
}

static int add_id(const char *id) {
    size_t namelen = strlen(id);
    for (int i = 0; i < id_exists_num; ++i)
        if (!strcmp(id_exists[i], id))
            return 0;
    if (id_exists_num < 256)
        memmove(id_exists[id_exists_num++], id,
                (namelen < 19 ? namelen : 19));
    return 1;
}

static void check_block(struct create_args *args) {
    struct stat st;
    args->blkfd = open(args->blk, O_RDWR);
    if (args->blkfd < 0 || fstat(args->blkfd, &st) < 0) {
        fprintf(stderr, "access %s error: %s\n", args->blk, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode)) {
        fprintf(stderr, "%s is not regular file or block device\n", args->blk);
        exit(EXIT_FAILURE);
    }
    if (FLOOR_OFFSET2(st.st_size, SECTOR_SHIFT)) {
        fprintf(stderr, "the size of device is not multiple times of %d\n", SIZE2(SECTOR_SHIFT));
        exit(EXIT_FAILURE);
    }
    args->blksize = st.st_size;
}

static void delete_all_ids(struct create_args *args) {
    DIR *dir = fdopendir(dup(args->iddirfd));
    struct dirent *e;
    if (!dir)
        return;
    while ((e = readdir(dir)))
        if (e->d_type == DT_REG)
            unlinkat(args->iddirfd, e->d_name, 0);
    closedir(dir);
}

static void check_master_pair(struct create_args *args) {
    int fd = open(args->masterfile, O_RDONLY);
    int flag = 0;
    struct stat st;
    char *buf = MAP_FAILED;
    size_t size;
    if (fd < 0) {
        flag = 1;
        if (errno != ENOENT ||
                (fd = open(args->masterfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
            fprintf(stderr, "open %s error: %s\n", args->masterfile, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    if (!flag) {
        if (fstat(fd, &st) < 0 ||
                (buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
            fprintf(stderr, "access %s error: %s\n", args->masterfile, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if ((args->pair = master_key_pair_read(buf, st.st_size))) {
            munmap(buf, st.st_size);
            close(fd);
            return;
        }
        fd = open(args->masterfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            fprintf(stderr, "open %s error: %s\n", args->masterfile, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    args->regenerate = 1;
    delete_all_ids(args);
    fprintf(stderr, "delete all old ids\n");
    fprintf(stderr, "generating new master key pair\n");
    if (!(args->pair = generate_master_key_pair(TYPE_ENCRYPT))) {
        fprintf(stderr, "read master key pair error\n");
        exit(EXIT_FAILURE);
    }
    size = master_key_pair_size(args->pair);
    buf = NEW(char, size);
    master_key_pair_write(args->pair, buf, size);
    ssize_t s = write(fd, buf, size);
    if (s < (int)size) {
        perror("write master key pair error");
        exit(EXIT_FAILURE);
    }
    if (buf != MAP_FAILED)
        munmap(buf, st.st_size);
    close(fd);
}

void check_args(int argc, char **argv, struct create_args *args) {
    opterr = 0;
    int ind;
    while ((ind = getopt_long(argc, argv, "m:b:t:d:h", opts, NULL)) != -1) {
        switch (ind) {
            case 'm': args->masterfile = optarg; break;
            case 'b': args->blk = optarg; break;
            case 't': args->ids = optarg; break;
            case 'd': args->iddir = optarg; break;
            case 'h': usage(); exit(EXIT_FAILURE);
            case '?': usage(); exit(EXIT_FAILURE);
            default:
                      break;
        }
    }
    if (!args->masterfile || !args->iddir || !args->ids || !args->blk) {
        usage();
        exit(EXIT_FAILURE);
    }
    check_ids(args);
    check_master_pair(args);
    check_block(args);
}

char *get_id(char **ptr, const char *delim, struct create_args *args) {
    char *cur = strsep(ptr, delim);
    struct crypto_file *fp;
    if (cur && add_id(cur)) {
        int idlen = strlen(cur);
        size_t fpsize = sizeof(struct crypto_file) + idlen - 1;
        fp = malloc(fpsize);
        memmove(fp->id, cur, idlen);
        fp->idlen = idlen;
        printf("%s\n", cur);
        struct private_key *priv = get_private_key(args->pair, cur, strlen(cur), TYPE_ENCRYPT);
        assert(private_key_size(priv) == sizeof(fp->priv));
        if (!priv)
            fprintf(stderr, "error while creating private key for %s\n", cur);
        else {
            int fd = openat(args->iddirfd, cur, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
            if (fd < 0)
                fprintf(stderr, "error while creating file %s\n", cur);
            private_key_write(priv, fp->priv, sizeof(fp->priv));
            write(fd, fp, fpsize);
            close(fd);
        }
    }
    return cur;
}

void create_device(struct create_args *args) {
    size_t idlen = strlen(args->ids);
    char *ids = NEW(char, idlen + 1);
    char *ptr = ids, *cur;
    char key[SM4_XTS_KEY_BYTE_SIZE] = {};
    getrandom(key, sizeof(key), 0);
    memmove(ids, args->ids, idlen);
    ids[idlen] = '\0';
    cur = get_id(&ptr, ",", args);
    while (cur) {
        struct block_header header = {};
        for (int i = 0; i < 3 && cur; ++i) {
            struct cipher *cipher = sm9_encrypt(
                    args->pair, cur, strlen(cur),
                    key, sizeof(key),
                    1, 0x100);
            if (!cipher)
                exit(EXIT_FAILURE);
            assert(ciphertext_size(cipher) == 160);
            ciphertext_write(cipher, (void *)header.ciphertext[i], 160);
            ++header.header_number;
            ciphertext_free(cipher);
            cur = get_id(&ptr, ",", args);
        }
        SET_FLAG(header.header_flag, HEADER_CUR_TYPE);
        if (cur)
            SET_FLAG(header.header_flag, HEADER_NXT_TYPE);
        if (write(args->blkfd, &header, sizeof(header)) < 0) {
            perror("write header error");
            exit(EXIT_FAILURE);
        }
    }
    free(ids);
}

static void free_args(struct create_args *args) {
    close(args->blkfd);
}

int main(int argc, char **argv) {
    sm9_init();
    struct create_args args = {};
    check_args(argc, argv, &args);
    create_device(&args);
    free_args(&args);
    sm9_release();
    return 0;
}
