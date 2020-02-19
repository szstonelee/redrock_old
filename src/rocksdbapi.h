#ifndef __ROCKSDBAPI_H
#define __ROCKSDBAPI_H

void rocksdbapi_init(int dbnum, char *root_path);
void rocksdbapi_teardown();

void rocksdbapi_read(int dbi, void *key, size_t key_len, void **val, size_t *val_len);
void rocksdbapi_write(int dbi, char *key, size_t key_len, char *val, size_t val_len);
size_t rocksdbapi_memory(void);
void rocksdbapi_createSnapshots(void);
void rocksdbapi_relaseSnapshots(void);
void rocksdbapi_read_from_snapshot(int dbi, void *key, size_t key_len, void **val, size_t *val_len);

#endif
