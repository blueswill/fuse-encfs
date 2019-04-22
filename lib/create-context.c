#include<unistd.h>
#include<fcntl.h>
#include<sys/random.h>
#include<sys/mman.h>
#include<string.h>
#include<dirent.h>
#include<gmodule.h>
#include<sys/stat.h>
#include<stdint.h>
#include<linux/fs.h>
#include<sys/ioctl.h>
#include"sm9.h"
#include"sm4.h"
#include"create-context.h"
#include"encfs_helper.h"
#include"config.h"

struct create_context {
    int blkfd, iddirfd;
    int regenerate;
    off_t blksize;
    struct master_key_pair *pair;
    GHashTable *id_hash;
};

static int check_id_dir(struct create_context *args) {
    int iddirfd = args->iddirfd;
    GHashTable *id_hash = args->id_hash;
    DIR *dir = fdopendir(dup(iddirfd));
    if (!dir)
        return -1;
    struct dirent *e;
    while ((e = readdir(dir))) {
        if (e->d_type == DT_REG &&
            g_str_has_suffix(e->d_name, PRIVATE_KEY_SUFFIX)) {
            if (args->regenerate) {
                unlinkat(iddirfd, e->d_name, 0);
            }
            else {
                gchar *str = g_strdup(e->d_name);
                str[strlen(str) - sizeof(PRIVATE_KEY_SUFFIX)] = '\0';
                g_hash_table_insert(id_hash, str, NULL);
            }
        }
    }
    closedir(dir);
    return 0;
}

static void create_private_key_file(const char *id, struct create_context *args) {
    char *str = g_strdup_printf("%s"PRIVATE_KEY_SUFFIX, id);
    int fd = openat(args->iddirfd, str, O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd < 0) {
        g_free(str);
        return;
    }
    struct crypto *crypto = crypto_new(args->pair, id);
    crypto_write_file(crypto, fd);
    close(fd);
    crypto_free(crypto);
    g_free(str);
}

static void add_id(const char *id, struct create_context *args) {
    char *str = g_strdup(id);
    GHashTable *id_hash = args->id_hash;
    if (g_hash_table_insert(id_hash, str, NULL)) {
        create_private_key_file(id, args);
    }
}

static int check_block(struct create_context *args) {
    struct stat st;
    int blkfd = args->blkfd;
    uint64_t size;
    if (blkfd < 0 || fstat(blkfd, &st) < 0 ||
        !S_ISBLK(st.st_mode) ||
        ioctl(blkfd, BLKGETSIZE64, &size) < 0 ||
        FLOOR_OFFSET2((uint32_t)size, SECTOR_SHIFT))
        return -1;
    args->blksize = st.st_size;
    return 0;
}

struct create_context *create_context_new(int blkfd, int iddirfd,
                                    struct master_key_pair *pair, int regenerate) {
    int ret = -1;
    struct create_context *args = g_new0(struct create_context, 1);
    if (!args)
        return NULL;
    args->blkfd = dup(blkfd);
    args->iddirfd = dup(iddirfd);
    args->id_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    args->pair = pair;
    args->regenerate = !!regenerate;
    if (check_id_dir(args) < 0 ||
        check_block(args) < 0)
        goto end;
    ret = 0;
end:
    if (ret < 0) {
        create_context_free(args);
        args = NULL;
    }
    return args;
}

void create_context_free(struct create_context *args) {
    if (!args)
        return;
    if (args->blkfd >= 0)
        close(args->blkfd);
    if (args->iddirfd >= 0)
        close(args->iddirfd);
    if (args->id_hash) {
        g_hash_table_remove_all(args->id_hash);
        g_hash_table_unref(args->id_hash);
    }
    master_key_pair_free(args->pair);
    g_free(args);
}

int create_context_create(struct create_context *args, const char **id_list) {
    const char **cur = id_list;
    char key[SM4_XTS_KEY_BYTE_SIZE] = {};
    getrandom(key, sizeof(key), 0);
    while (*cur) {
        struct block_header header = {};
        for (int i = 0; i < 3 && *cur; ++i) {
            struct cipher *cipher =
                sm9_encrypt(args->pair, *cur, strlen(*cur),
                            key, sizeof(key),
                            1, 0x100);
            if (!cipher)
                return -1;
            ciphertext_write(cipher, (void *)header.ciphertext[i], 160);
            ++header.header_number;
            ciphertext_free(cipher);
            add_id(*cur, args);
            ++cur;
        }
        SET_HEADER_FLAG(header.fs_flag, !!cur);
        if (write(args->blkfd, &header, sizeof(header)) < 0) {
            return -1;
        }
    }
    return 0;
}
