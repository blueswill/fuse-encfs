#define FUSE_USE_VERSION 31
#include<fuse3/fuse.h>
#include<stdio.h>
#include<unistd.h>
#include<locale.h>
#include<getopt.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<pthread.h>
#include<sys/errno.h>
#include<openssl/evp.h>
#include<openssl/rand.h>

#define SECTOR_SHIFT 9
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
#define SECTOR_MASK (SECTOR_SIZE - 1)
#define GET_SECTOR(offset) ((offset) >> SECTOR_SHIFT)
#define SECTOR_OFFSET(offset) ((offset) & SECTOR_MASK)
#define SECTOR_DOWN(x) ((x) & ~SECTOR_MASK)
#define SECTOR_UP(x) ((((x) - 1) | SECTOR_MASK) + 1)


void usage(struct fuse_args *args) {
    fputs("usage:\n", stderr);
    fputs("    --help                   print this help message.\n", stderr);
    fputs("    --block-device=[device]  set target block device.\n", stderr);
    fputs("    --key=[keyfile]          set aes-256-xts key for block device.\n", stderr);
    fputs("    --iv=[ivfile]            set aes-256-xts iv for block device.\n", stderr);
    assert(fuse_opt_add_arg(args, "--help") == 0);
}

struct user_args {
    const char *block_device;
    const char *key_file;
    const char *iv_file;
    int show_help;
};

struct encfs_context {
    struct user_args args;
    unsigned char key[64];
    unsigned char iv[16];
    EVP_CIPHER_CTX *ctx;
    pthread_mutex_t mutex;
    int blk_fd;
    struct stat st;
};

static struct encfs_context context = {
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

#define offsetof(type, elem) \
    ((unsigned long)&(((type *)0)->elem))
#define OPTION(t, p) \
    { t, offsetof(struct user_args, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("--block-device=%s", block_device),
    OPTION("--key=%s", key_file),
    OPTION("--iv=%s", iv_file),
    OPTION("--help", show_help),
    FUSE_OPT_END
};

void *encfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    cfg->kernel_cache = 1;
    context.ctx = EVP_CIPHER_CTX_new();
}

int encfs_getattr(const char *path, struct stat *st, struct fuse_file_info *info) {
    (void)info;
    memset(st, 0, sizeof(*st));
    if (!strcmp(path, "/")) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else if (!strcmp(path + 1, "target")) {
        struct stat blkst;
        fstat(context.blk_fd, &blkst);
        memmove(st, &blkst, sizeof(*st));
    }
    else
        return -ENOENT;
    return 0;
}

int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    if (strcmp(path, "/"))
        return -ENOENT;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "target", NULL, 0, 0);
    return 0;
}

int encfs_open(const char *path, struct fuse_file_info *fi) {
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    return 0;
}

static int generate_iv(unsigned char *iv, off_t sector) {
    unsigned char ivp[sizeof(context.iv)] = {};
    int len = 0;
    memmove(ivp, &sector, sizeof(sector));
    if (EVP_EncryptInit_ex(context.ctx, EVP_aes_256_cbc(), NULL,
                context.key, context.iv) != 1)
        return -1;
    EVP_CIPHER_CTX_set_padding(context.ctx, 0);
    if (EVP_EncryptUpdate(context.ctx, iv, &len, ivp, sizeof(context.iv)) != 1)
        return -1;
    if (EVP_EncryptFinal_ex(context.ctx, iv + len, &len) != 1)
        return -1;
    return 0;
}

static int decrypt_sector(unsigned char *plaintext, off_t sector) {
    int len;
    unsigned char iv[sizeof(context.iv)];
    unsigned char ciphertext[SECTOR_SIZE];
    if (generate_iv(iv, sector) < 0)
        return -1;
    if (pread(context.blk_fd, ciphertext, SECTOR_SIZE, sector * SECTOR_SIZE) != SECTOR_SIZE)
        return -1;
    if (EVP_DecryptInit_ex(context.ctx, EVP_aes_256_xts(), NULL, context.key, iv) != 1)
        return -1;
    if (EVP_DecryptUpdate(context.ctx, plaintext, &len,
                ciphertext, SECTOR_SIZE) != 1)
        return -1;
    if (EVP_DecryptFinal_ex(context.ctx, plaintext + len, &len) != 1)
        return -1;
    return 0;
}

static int encrypt_sector(unsigned char *ciphertext, const unsigned char *plaintext, off_t sector) {
    int len;
    unsigned char iv[sizeof(context.iv)];
    if (generate_iv(iv, sector) < 0)
        return -1;
    if (EVP_EncryptInit_ex(context.ctx, EVP_aes_256_xts(), NULL, context.key, iv) != 1)
        return -1;
    if (EVP_EncryptUpdate(context.ctx, ciphertext, &len, plaintext, SECTOR_SIZE) != 1)
        return -1;
    if (EVP_EncryptFinal_ex(context.ctx, ciphertext + len, &len) != 1)
        return -1;
    return 0;
}

static ssize_t read_sector(char *buf, off_t sector, off_t offset, size_t size) {
    unsigned char plain[SECTOR_SIZE];
    if (size + offset > SECTOR_SIZE)
        size = SECTOR_SIZE - offset;
    if (decrypt_sector(plain, sector) < 0)
        return -1;
    memmove(buf, plain + offset, size);
    return size;
}

static ssize_t write_sector(char *cipher, const char *buf, off_t sector,
        off_t offset, size_t size) {
    char sector_data[SECTOR_SIZE];
    const char *wait = buf;
    if (size + offset > SECTOR_SIZE)
        size = SECTOR_SIZE - offset;
    if (offset != 0 || size != SECTOR_SIZE) {
        if (read_sector(sector_data, sector, offset, size) < 0)
            return -1;
        memmove(sector_data + offset, buf + offset, size);
        wait = sector_data;
    }
    if (encrypt_sector((unsigned char *)cipher, (unsigned char *)wait, sector) < 0)
        return -1;
    return size;
}

static int __encfs_read(char *buf, size_t size, off_t offset) {
    size_t len = 0;
    ssize_t rd;
    while (len < size) {
        rd = read_sector(buf + len, GET_SECTOR(offset + len),
                SECTOR_OFFSET(offset + len), size - len);
        if (rd < 0)
            return -1;
        len += rd;
    }
    return len;
}

static int __encfs_write(const char *buf, size_t size, off_t offset) {
    size_t len = 0, i = 0;
    ssize_t rd;
    size_t alloc = sizeof(char) * (SECTOR_UP(offset + size) - SECTOR_DOWN(offset));
    char *cipher = (char *)malloc(alloc);
    if (!cipher)
        return -1;
    while (len < size) {
        rd = write_sector(cipher + i * SECTOR_SIZE, buf + len, GET_SECTOR(offset + len),
                SECTOR_OFFSET(offset + len), size - len);
        if (rd < 0) {
            free(cipher);
            return -1;
        }
        len += rd;
        ++i;
    }
    pwrite(context.blk_fd, cipher, len, SECTOR_DOWN(offset));
    free(cipher);
    return len;
}

int encfs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    pthread_mutex_lock(&context.mutex);
    if (offset >= context.st.st_size)
        return 0;
    if (offset + size > context.st.st_size)
        size = context.st.st_size - offset;
    if ((ret = __encfs_read(buf, size, offset)) < 0) {
        ret = -EIO;
        goto free_lock;
    }
free_lock:
    pthread_mutex_unlock(&context.mutex);
    return ret;
}

int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    int ret = -EIO;
    (void)fi;
    if (strcmp(path, "/target"))
        return -ENOENT;
    if (offset + size > (size_t)context.st.st_size)
        return -ENOSPC;
    pthread_mutex_lock(&context.mutex);
    if ((ret = __encfs_write(buf, size, offset)) < 0) {
        ret = -EIO;
        goto free_lock;
    }
free_lock:
    pthread_mutex_unlock(&context.mutex);
    return ret;
}

void encfs_destroy(void *private_data) {
    (void)private_data;
    close(context.blk_fd);
}

#define STR_EMPTY(str) (!(str) || *(str) == '\0')
int check_args(struct encfs_context *context, struct fuse_args *args) {
    int keyfd, ivfd;
    ssize_t rd;
    if (STR_EMPTY(context->args.block_device) ||
            STR_EMPTY(context->args.key_file) ||
            STR_EMPTY(context->args.iv_file)) {
        usage(args);
        return -1;
    }
    if ((context->blk_fd = open(context->args.block_device, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", context->args.block_device,
                strerror(errno));
        return -1;
    }
    if ((keyfd = open(context->args.key_file, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", context->args.key_file,
                strerror(errno));
        goto free_blk;
    }
    if ((ivfd = open(context->args.iv_file, O_RDWR)) < 0) {
        fprintf(stderr, "%s open error: %s\n", context->args.iv_file,
                strerror(errno));
        goto free_key;
    }
    if ((rd = read(keyfd, context->key, sizeof(context->key))) < 0) {
        fprintf(stderr, "%s read error: %s\n", context->args.key_file,
                strerror(errno));
        goto free_iv;
    }
    if ((rd = read(ivfd, context->iv, sizeof(context->iv))) < 0) {
        fprintf(stderr, "%s read error: %s\n", context->args.iv_file,
                strerror(errno));
        goto free_iv;
    }
    if (fstat(context->blk_fd, &context->st) < 0 ||
            SECTOR_OFFSET(context->st.st_size) != 0)
        goto free_iv;
    return 0;
free_iv:
    close(ivfd);
free_key:
    close(keyfd);
free_blk:
    close(context->blk_fd);
    return -1;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int ret = EXIT_FAILURE;
    if (fuse_opt_parse(&args, &context.args, option_spec, NULL) == -1 ||
            check_args(&context, &args))
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
    ret = fuse_main(args.argc, args.argv, &oprs, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
