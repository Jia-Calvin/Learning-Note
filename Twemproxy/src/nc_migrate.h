#ifndef _NC_MIGRATE_H_
#define _NC_MIGRATE_H_

#include <nc_core.h>
#include <nc_string.h>

/**
在缓存的分布式模型中，没有slot的概念；在迁移时引入 slot只是为了将迁移过程切分成一个个小的
阶段；定义迁移时有1024个slot；
迁移是slot 0开始到slot 1024 完成；通过计算当前key所在的slot，决定它的访问策略；
  	access_slot  < slot_migerting: routing_new;
  	access_slot  > slot_migerting: routing_old;
  	access_slot = cur_migrate: routing_old first, than routing_new 
      使用封装后的命令来访问redis实例：SLOTSMGRT-EXEC-WRAPPER Keys
      首次总是访问老节点；
      if got，直接返回；
      if moved，访问新的节点；
      if retry，再次访问正在迁移的节点；
*/
#define MIGRATE_SLOTS_MASK 0x000003ff
#define MIGRATE_SLOTS_SIZE (HASH_SLOTS_MASK + 1)

#define SLOT_WRAPPER "$22\r\nSLOTSMGRT-EXEC-WRAPPER\r\n"
#define SLOT_WRAPPER_LEN  (sizeof(SLOT_WRAPPER)-1)

// eg: migrating:1, migrating_slot:1023
#define MIGRATE_STATUS_LEN  50


int migrate_slot_num(uint8_t *key, uint32_t keylen);
struct server_pool * get_access_pool(struct context *ctx, struct conn *c_conn,
                                        uint8_t **pkey, uint32_t keylen, struct msg *msg);

rstatus_t migrate_prepare(struct string* manage_cmd, struct string* result);
rstatus_t migrate_begin(struct string* manage_cmd, struct string* result);
rstatus_t migrate_slot(struct string* manage_cmd, struct string* result);
rstatus_t migrate_discard(struct string* manage_cmd, struct string* result);
rstatus_t migrate_done(struct string* manage_cmd, struct string* result);
rstatus_t migrate_status(struct string* manage_cmd, struct string* result);
int set_double_cluster_mode(struct context *ctx, struct string value);
rstatus_t double_cluster_mode(struct string* manage_cmd, struct string* result);
rstatus_t unwrapper_migrating_response(struct msg *msg);
void unwrapper_migrating_key(struct msg *msg);

#endif
