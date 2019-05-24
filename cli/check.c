#include<stdio.h>
#include<gmodule.h>
#include<getopt.h>
#include<fcntl.h>
#include<errno.h>
#include<json-glib/json-glib.h>
#include"check-context.h"
#include"encfs_helper.h"
#include"helper.h"

static struct option opts[] = {
    {"config", required_argument, NULL, 'c'},
    {"device", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'}
};

struct user_args {
    const char *config;
    const char *device;
    int show_help;
};

struct config {
    struct crypto *crypto;
};

static int process_args(int argc, char **argv, struct user_args *args) {
    opterr = 0;
    int ind;
    while ((ind = getopt_long(argc, argv, "c:d:gh", opts, NULL)) != -1) {
        switch (ind) {
            case 'c': args->config = optarg; break;
            case 'h': args->show_help = 1; break;
            case 'd': args->device = optarg; break;
            case '?': return -1;
        }
    }
    return 0;
}

static void print_help(const char *name) {
    fprintf(stderr, "%s [-gh] -c CONFIG -d DEVICE\n", name);
    fputs("Usage:\n"
          "    --config=CONFIG, -c          set configuration position\n"
          "    --device=DEVICE, -d          select DEVICE\n"
          "    -help, -h                    show this help message\n",
          stderr);
}

static void _config_get_string(JsonObject *obj,
                               const gchar *name, gchar const **addr,
                               ...
                               /* NULL */) {
    va_list ap;
    va_start(ap, addr);
    while (name) {
        *addr = json_object_get_string_member(obj, name);
        name = va_arg(ap, const gchar *);
        addr = va_arg(ap, gchar const **);
    }
    va_end(ap);
}

static int config_get(const char *config, struct config *user_config) {
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) err = NULL;
    const gchar *owner, *primary, *private, *public, *object;
    const gchar *pkey;
    struct tpm_args args;
    if (!json_parser_load_from_file(parser, config, &err)) {
        g_warning(err->message);
        return -1;
    }
    JsonNode *node = json_parser_get_root(parser);
    if (json_node_get_node_type(node) != JSON_NODE_OBJECT) {
        g_warning("config root is not OBJECT");
        return -1;
    }
    _config_get_string(json_node_get_object(node),
                       "owner", &owner, "primary", &primary,
                       "private", &private, "public", &public,
                       "object", &object, "pkey", &pkey,
                       NULL);
    if (!private || !public) {
        g_warning("no private key nor public key provided");
        return -1;
    }
    if (!pkey) {
        g_warning("no SM9 private key provided");
        return -1;
    }
    if (!tpm_args_init(&args, owner, primary, private, public, object))
        return -1;
    int fd = open(pkey, O_RDONLY);
    if (fd < 0) {
        g_warning("read %s error: %s", pkey, g_strerror(errno));
        goto free_tpm;
    }
    user_config->crypto = crypto_read_file(fd, _decrypt, &args);
    close(fd);
    if (!user_config->crypto) {
        g_warning("%s is not a valid pkey file", pkey);
        return -1;
    }
    tpm_args_reset(&args);
    return 0;
free_tpm:
    tpm_args_reset(&args);
    return -1;
}

static int check_device(const char *device, struct crypto *crypto) {
    int fd = open(device, O_RDONLY | O_EXCL);
    if (fd < 0) {
        g_warning("open %s error: %s", device, g_strerror(errno));
        return -1;
    }
    struct check_context *ctx = check_context_new(fd, crypto);
    if (!ctx)
        return -1;
    int ret = check_context_do_check(ctx);
    check_context_free(ctx);
    return ret;
}

int main(int argc, char **argv) {
    struct user_args args = {};
    struct config config = {};
    int ret = 1;
    sm9_init();
    int status = process_args(argc, argv, &args);
    if (status < 0 || args.show_help || !args.device || !args.config) {
        print_help(argv[0]);
        goto end;
    }
    if (config_get(args.config, &config) < 0)
        goto end;
    ret = check_device(args.device, config.crypto);
end:
    sm9_release();
    return ret <= 0;
}
