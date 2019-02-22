#ifndef _NC_MANAGE_H_
#define _NC_MANAGE_H_

#include <nc_core.h>
#include <nc_string.h>

// proto:<2_byte_cmd_len><cmd> eg: 15migrate_prepare
#define MANAGE_CMD_BYTE_LEN  2

typedef enum manage_type {
    MANAGE_COMMAND_UNKNOWN,
    REDIS_MIGRATE_PREPARE,         /* redis migrate commonds */
    REDIS_MIGRATE_BEGIN,
    REDIS_MIGRATE_SLOT,            /* eg:migrate_slot:100 */
    REDIS_MIGRATE_DISCARD,
    REDIS_MIGRATE_DONE,
    REDIS_MIGRATE_STATUS,
    DOUBLE_CLUSTER_MODE,
    MANAGE_COMMAND_END,
} manage_type_t;

typedef rstatus_t (*manage_cmd_handler_t)(struct string *input, struct string* output);

typedef struct manage_type_info {
  manage_type_t type;
  const char *str;
  manage_cmd_handler_t manage_cmd_handler;     
} manage_type_info_t;
extern manage_type_info_t mtype_info[];
extern const uint MANAGE_TYPE_LEN;


rstatus_t manager_init(struct context *ctx, char *manage_addr);
void manager_deinit(struct context *ctx);
rstatus_t manager_recv(struct context *ctx, struct conn *conn);

void command_processor(struct string* manage_cmd, struct string* result);

bool manage_type_check(void);
const char * manage_type_string(manage_type_t type);


#endif
