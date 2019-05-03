#ifndef UTILITY_H
#define UTILITY_H

#include<gmodule.h>

gchar *unfused_path(const gchar *path);
int _decrypt(const gchar *in, size_t inlen, char **out, size_t *outlen, void *userdata);
int _encrypt(const gchar *in, size_t inlen, char **out, size_t *outlen, void *userdata);
gchar *_get_password(void);

#endif
