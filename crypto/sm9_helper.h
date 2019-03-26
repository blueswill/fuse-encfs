#ifndef SM9_HELPER_H
#define SM9_HELPER_H

#include<stddef.h>
#include<stdint.h>
#include"miracl.h"
#include"encfs_helper.h"
#include"sm9.h"

struct _string {
    uint8_t *buf;
    size_t size;
};

struct master_key_pair {
    enum GEN_TYPE type;
    big priv;
    epoint *pub;
};

struct private_key {
    enum GEN_TYPE type;
    ecn2 e;
};

enum HID {
    HID_ENCRYPT = 0x03
};

struct cipher {
    struct _string text;
};

int H1(const struct _string *id, uint8_t hid, big *b);
int H2(const struct _string *id, uint8_t hid, big *b);

#define KDF_EXTRA_BYTE_LEN 4
int KDF(struct _string *z, size_t hlen, struct _string *ret);

void report_error(enum sm9_error err);

#define get_string(b, s) {\
    .buf = (void *)(b),\
    .size = (s)\
}

#define NEW_STRING(sptr,l) ({\
        struct _string *ptr = (sptr);\
        ptr->size = (l);\
        ptr->buf = NEW(uint8_t, ptr->size);\
        })

#define init_big(x) ((x) = mirvar(0))
#define release_big(x) mirkill(x)
#define init_zzn2(x) (init_big((x).a), init_big((x).b))
#define release_zzn2(x) (release_big((x).a), release_big((x).b))
#define init_epoint(e) ((e) = epoint_init())
#define release_epoint(e) epoint_free(e)
#define init_ecn2(e) do {\
    init_zzn2((e).x);\
    init_zzn2((e).y);\
    init_zzn2((e).z);\
    (e).marker = MR_EPOINT_INFINITY;\
} while (0)
#define release_ecn2(e) do {\
    release_zzn2((e).x);\
    release_zzn2((e).y);\
    release_zzn2((e).z);\
} while (0)

#define read_big(r, b, blen) bytes_to_big(blen, (const char *)(b), r);
#define big_size(b) ((b)->len * sizeof((b)->w))

int write_big(big r, struct _string *s);
#define write_big_buf(r, buf, size) \
    big_to_bytes((size), (r), (void *)(buf), 0)
size_t epoint_size(epoint *e, big *x, big *y);
int write_epoint(epoint *e, struct _string *s);
int write_epoint_buf(epoint *e, uint8_t *buf, size_t size);
#endif
