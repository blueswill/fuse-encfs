#include<stdio.h>
#include<gmodule.h>
#include<getopt.h>
#include<fcntl.h>
#include<errno.h>
#include<json-glib/json-glib.h>
#include"check-context.h"
#include"encfs_helper.h"

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

static int config_get(const char *config, struct config *user_config) {
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) err = NULL;
    if (!json_parser_load_from_file(parser, config, &err)) {
        g_warning(err->message);
        return -1;
    }
    g_autoptr(JsonPath) path = json_path_new();
    if (!json_path_compile(path, "$.pkey", &err)) {
        g_warning(err->message);
        return -1;
    }
    g_autoptr(JsonNode) node = json_path_match(path, json_parser_get_root(parser));
    JsonArray *arr = json_node_get_array(node);
    if (json_array_get_length(arr) != 1) {
        g_warning("no pkey found in configuration");
        return -1;
    }
    JsonNode *pkey = json_array_get_element(arr, 0);
    const gchar *f = json_node_get_string(pkey);
    int fd = open(f, O_RDONLY);
    if (fd < 0) {
        g_warning("read %s error: %s", f, g_strerror(errno));
        return -1;
    }
    user_config->crypto = crypto_read_file(fd);
    close(fd);
    if (!user_config->crypto) {
        g_warning("%s is not a valid pkey file", f);
        return -1;
    }
    return 0;
}

static int check_device(const char *device, struct crypto *crypto) {
    int fd = open(device, O_RDONLY);
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
