#ifndef __ROCKSERDES_H
#define __ROCKSERDES_H

#include "server.h"

robj *desString(char *s, size_t len);
sds serObject(robj *o);
robj *desObject(void *buf, size_t len);

void _test_ser_des_string(void);
void _test_ser_des_list(void);
void _test_ser_des_set(void);
void _test_ser_des_hash(void);
void _test_ser_des_zset(void);

#endif
