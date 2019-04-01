#include<fcntl.h>
#include<string.h>
#include<sys/stat.h>
#include<unistd.h>
#include<sys/mman.h>
#include"sm9_helper.h"
#include"sm9.h"
#include"encfs.h"
#include"sm4.h"

void create_swhc(void) {
    struct master_key_pair *pair = generate_master_key_pair(TYPE_ENCRYPT);
    size_t size = master_key_pair_size(pair);
    char *buf = NEW(char, size);
    master_key_pair_write(pair, buf, size);
    int fd = open("tmpmaster", O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    write(fd, buf, size);
    close(fd);
    struct crypto_file *fp = malloc(sizeof(struct crypto_file) + 3);
    struct private_key *key = get_private_key(pair, "swhc", 4, TYPE_ENCRYPT);
    private_key_write(key, fp->priv, sizeof(fp->priv));
    memmove(fp->id, "swhc", 4);
    fp->idlen = 4;
    fd = open("tmppriv", O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    write(fd, fp, sizeof(struct crypto_file) + 3);
    close(fd);
}

void write_crypto(int fd, struct private_key *c, const char *id, int idlen) {
    size_t size = sizeof(struct crypto_file) + idlen - 1;
    struct crypto_file *fp = malloc(size);
    private_key_write(c, fp->priv, sizeof(fp->priv));
    fp->idlen = idlen;
    memmove(fp->id, id, idlen);
    write(fd, fp, size);
}

struct private_key *read_crypto(int fd) {
    struct stat st;
    fstat(fd, &st);
    struct crypto_file *fp = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    struct private_key *pkey = private_key_read(fp->priv, sizeof(fp->priv));
    munmap(fp, st.st_size);
    return pkey;
}

struct master_key_pair *read_master_key_pair(int fd) {
    struct stat st;
    fstat(fd, &st);
    char *fp = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    struct master_key_pair *pkey = master_key_pair_read(fp, st.st_size);
    munmap(fp, st.st_size);
    return pkey;
}

struct cipher *read_ciphertext() {
    int fd = open("device/dev", O_RDONLY);
    struct block_header *header = mmap(NULL, sizeof(struct block_header), PROT_READ, MAP_PRIVATE, fd, 0);
    struct cipher *cipher = ciphertext_read((void *)header->ciphertext[0], 160);
    munmap(header, sizeof(struct block_header));
    return cipher;
}

int main(void) {
    sm9_init();
    create_swhc();
    struct private_key *pkey = read_crypto(open("id/swhc", O_RDONLY));
    struct cipher *c1 = read_ciphertext();
    size_t size;
    //pair = generate_master_key_pair(TYPE_ENCRYPT);
    char key[SM4_XTS_KEY_BYTE_SIZE] = {};
    char *out;
    //pkey = get_private_key(pair, "swhc", 4, TYPE_ENCRYPT);
    int ret = sm9_decrypt(pkey, c1, "swhc", 4, 1, 0x100, &out, &size);
    for (unsigned i = 0; i < size; ++i)
        printf("%02x", (uint8_t)out[i]);
    putchar('\n');
    sm9_release();
    return 0;
}

/*
int main(void) {
    sm9_init();
    struct master_key_pair *pair = generate_master_key_pair(TYPE_ENCRYPT);
    struct private_key *priv = get_private_key(pair, "swhc", 4, TYPE_ENCRYPT);
    char key[SM4_XTS_KEY_BYTE_SIZE] = {};
    struct cipher *c = sm9_encrypt(pair, "swhc", 4, key, sizeof(key), 1, 0x100);
    size_t size = ciphertext_size(c);
    char *cbuf = NEW(char, size);
    ciphertext_write(c, cbuf, size);
    for (unsigned i = 0; i < size; ++i) {
        printf("%02x", (uint8_t)cbuf[i]);
    }
    printf("\n");
    int fd = open("tmpmaster", O_WRONLY | O_CREAT | O_TRUNC);
    char *buf;
    int ret = sm9_decrypt(priv, c, "swhc", 4, 1, 0x100, &buf, &size);
    printf("%d\n", ret);
    for (size_t i = 0; i < size; ++i)
        printf("%02x", buf[i]);
    printf("\n");
    write_crypto(fd, priv, "swhc", 4);
    close(fd);
    fd = open("tmpmaster", O_RDONLY);
    priv = read_crypto(fd);
    sm9_decrypt(priv, c, "swhc", 4, 1, 0x100, &buf, &size);
    for (size_t i = 0; i < size; ++i)
        printf("%02x", buf[i]);
    sm9_release();
}
*/
