// hash tool

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include <nc_core.h>
#include <nc_conf.h>
#include <nc_server.h>

static char short_options[] = "hc:k:";

static void
show_usage(void)
{
    log_stderr("Used for query key distribute in which node" CRLF
                "Usage: key_distribute -c conf_file -k query_keys(multi keys split by ^)"
             );
    exit(1);
}


static void
query_distribute(struct context *ctx, uint8_t *key_pos, uint32_t keylen)
{
    struct string key;
    string_init(&key);
    string_copy(&key, key_pos, keylen);
    struct server_pool * pool = array_top(&ctx->pool);
    /* from a given {key, keylen} pick a server from pool */
    struct server * s = server_pool_server(pool, key.data, keylen);
    printf("key:%s -> name:%s pname:%s\n",
            key.data, s->name.data, s->pname.data);
}

int
main(int argc, char **argv)
{
    struct instance *nci = &global_nci;
    int c;
    struct string keys;
    string_init(&keys);
    for (;;) {
        c = getopt_long(argc, argv, short_options, NULL, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }
        switch (c) {
        case 'c':
            nci->conf_filename = optarg;
            break;
        case 'k':
            keys.data = (void*)optarg;
            keys.len = (uint32_t)nc_strlen(keys.data);
            break;
        case 'h':
            show_usage();
        default:
            log_stderr("key_distribute: invalid option -- '%c'", optopt);
            return NC_ERROR;
        }
    }
    if (nci->conf_filename == NULL || keys.len == 0){
        log_stderr("need input conf file and query keys.");
        show_usage();
        return NC_ERROR;
    }

    struct context *ctx;
    ctx = nc_alloc(sizeof(*ctx));
    ctx->cf = conf_create(nci->conf_filename);
    struct conf_pool *cp = array_top(&ctx->cf->pool);
    cp->read_prefer = READ_PREFER_MASTER;
    cp->auto_eject_hosts = 0;
    /* initialize server pool from configuration */
    rstatus_t status = server_pool_init(&ctx->pool, &ctx->cf->pool, ctx);
    if (status != NC_OK) {
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NC_ERROR;
    }

    uint8_t * current_pos = keys.data;
    while(true){
        uint8_t *split_pos = nc_strchr(current_pos, keys.data+keys.len, '^');
        if(split_pos == NULL){
            query_distribute(ctx, current_pos, (uint32_t)nc_strlen(current_pos));
            break;
        }else{         
            uint32_t keylen = (uint32_t)(split_pos - current_pos);
            query_distribute(ctx, current_pos, keylen);
            current_pos = split_pos + 1;
        }
    }
    return NC_OK;
}