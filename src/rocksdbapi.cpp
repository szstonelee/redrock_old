#include <experimental/filesystem>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"

extern "C" void *zmalloc(size_t size);

extern "C" void rocksdbapi_init(int dbnum, char *root_path);
extern "C" void rocksdbapi_teardown();
extern "C" void rocksdbapi_read(int dbi, void *key, size_t key_len, void **val, size_t *val_len);
extern "C" void rocksdbapi_write(int dbi, char *key, size_t key_len, char *val, size_t val_len);
extern "C" size_t rocksdbapi_memory(void);
extern "C" void rocksdbapi_createSnapshots(void);
extern "C" void rocksdbapi_releaseAllSnapshots(void);
extern "C" void rocksdbapi_read_from_snapshot(int dbi, void *key, size_t key_len, void **val, size_t *val_len);

#define ROCKDB_WRITE_BUFFER_SIZE    16
#define ROCKDB_BLOCK_CACHE_SIZE    0
#define ROCKDB_BLOCK_SIZE   16
#define MAX_WRITE_BUFFER_NUMBER 1

std::vector<rocksdb::DB*> rocksdb_all_dbs;
std::vector<const rocksdb::Snapshot*> rocksdb_all_snapshots;
std::string rocksdb_root_path;

void rocksdbapi_createSnapshots(void) {    
    for (int i = 0; i < rocksdb_all_dbs.size(); ++i) {
        rocksdb::DB* db = rocksdb_all_dbs[i];
        rocksdb::Snapshot const *saved = rocksdb_all_snapshots[i];

        assert(saved == NULL);
        if (db) {
            rocksdb::Snapshot const *snapshot = db->GetSnapshot();
            rocksdb_all_snapshots[i] = snapshot;
        }
    }
}

void rocksdbapi_releaseAllSnapshots(void) {
    for (int i = 0; i < rocksdb_all_dbs.size(); ++i) {
        rocksdb::DB *db = rocksdb_all_dbs[i];
        if (db) {
            rocksdb::Snapshot const *snapshot = db->GetSnapshot();
            if (snapshot) 
                db->ReleaseSnapshot(snapshot);
        }
        rocksdb_all_snapshots[i] = NULL; 
    }
}

size_t rocksdbapi_memory(void) {    
    return (ROCKDB_WRITE_BUFFER_SIZE + ROCKDB_BLOCK_CACHE_SIZE) << 20;
}

rocksdb::DB* open_if_not_exist(int dbi) {
    assert(dbi >= 0 && rocksdb_all_dbs.size() > dbi);

    rocksdb::DB* db = rocksdb_all_dbs[dbi];

    if (db) return db;

    rocksdb::Options options;
    options.create_if_missing = true;
    
    rocksdb::BlockBasedTableOptions table_options;
	// table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
	table_options.block_cache = rocksdb::NewLRUCache(ROCKDB_BLOCK_CACHE_SIZE << 20);
	table_options.block_size = ROCKDB_BLOCK_SIZE << 10;
	table_options.pin_l0_filter_and_index_blocks_in_cache = true;
	options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
    options.write_buffer_size = ROCKDB_WRITE_BUFFER_SIZE << 20;
    options.max_write_buffer_number = MAX_WRITE_BUFFER_NUMBER;
    options.level_compaction_dynamic_level_bytes = true;
    options.bytes_per_sync = 0;
    options.compaction_pri = rocksdb::kMinOverlappingRatio;
    options.max_background_jobs = 1;
    options.max_open_files = -1;
    options.compaction_style = rocksdb::kCompactionStyleLevel;
    options.level0_file_num_compaction_trigger = 10;
    options.level0_slowdown_writes_trigger = 20;
    options.level0_stop_writes_trigger = 40;
    options.max_bytes_for_level_base = 512 << 20;
    options.max_bytes_for_level_multiplier = 10;
 
    // options.compression = rocksdb::kLZ4Compression;
    // options.compression = rocksdb::kSnappyCompression;
    options.compression_opts.level = rocksdb::CompressionOptions::kDefaultCompressionLevel;

    std::string path(rocksdb_root_path);
    path += std::to_string(dbi);
    rocksdb::Status status = rocksdb::DB::Open(options, path, &db);
    // assert(status.ok());
    rocksdb_all_dbs[dbi] = db;
    return db;
}

void rocksdbapi_init(int dbnum, char *root_path) {
    assert(dbnum > 0 && root_path && rocksdb_all_dbs.empty() && rocksdb_root_path.empty());

    rocksdb_root_path = std::string(root_path); 

    if (!std::experimental::filesystem::exists(rocksdb_root_path)) {
        std::experimental::filesystem::create_directory(rocksdb_root_path);
    } else {
        for (int i = 0; i < dbnum; ++i) {
            std::experimental::filesystem::path dir = root_path;
            dir += std::to_string(i);
            std::error_code errorCode;
            if (std::experimental::filesystem::remove_all(dir, errorCode) < 0) {
                std::cout << "rocksdbapi_init(), remove rock sub director {" <<  i << "} failed with errorcode " << errorCode << std::endl;
                return;
            }
        }
    }

    for (int i = 0; i < dbnum; ++i) {
        rocksdb_all_dbs.push_back(NULL);
        rocksdb_all_snapshots.push_back(NULL);
    }

    open_if_not_exist(0);
}

void rocksdbapi_teardown() {
    for (auto db : rocksdb_all_dbs) {
        if (db) delete db;
    }
}

/* if not found, val is NULL */
void rocksdbapi_read(int dbi, void *key, size_t key_len, void **val, size_t *val_len) {
    assert(dbi >= 0 && dbi < rocksdb_all_dbs.size());
    assert(key && key_len && val && val_len);

    rocksdb::DB* db = open_if_not_exist(dbi);
    assert(db);

    std::string rock_val;
    rocksdb::Slice rock_key((char*)key, key_len);
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(), rock_key, &rock_val);

    if (s.IsNotFound()) {
        *val = NULL;
        return;
    }

    assert(s.ok());

    void* new_heap_memory = zmalloc(rock_val.size());
    memcpy(new_heap_memory, rock_val.data(), rock_val.size());
    *val = new_heap_memory;
    *val_len = rock_val.size();
}

/* if not found, val is NULL */
void rocksdbapi_read_from_snapshot(int dbi, void *key, size_t key_len, void **val, size_t *val_len) {
    assert(dbi >= 0 && dbi < rocksdb_all_dbs.size());
    assert(key && key_len && val && val_len);

    rocksdb::DB *db = rocksdb_all_dbs[dbi];
    assert(db);
    rocksdb::Snapshot const *snapshot = rocksdb_all_snapshots[dbi];
    assert(snapshot);

    std::string rock_val;
    rocksdb::Slice rock_key((char*)key, key_len);
    rocksdb::ReadOptions option = rocksdb::ReadOptions();
    option.snapshot = snapshot;
    rocksdb::Status s = db->Get(option, rock_key, &rock_val);

    if (s.IsNotFound()) {
        *val = NULL;
        return;
    }

    assert(s.ok());

    void* new_heap_memory = zmalloc(rock_val.size());
    memcpy(new_heap_memory, rock_val.data(), rock_val.size());
    *val = new_heap_memory;
    *val_len = rock_val.size();
}

void rocksdbapi_write(int dbi, char *key, size_t key_len, char *val, size_t val_len) {
    assert(dbi >= 0 && dbi < rocksdb_all_dbs.size());
    assert(key && key_len && val && val_len);

    rocksdb::DB* db = open_if_not_exist(dbi);
    assert(db);

    rocksdb::WriteOptions write_opts;
    write_opts.disableWAL = true;
    rocksdb::Status s = 
        db->Put(write_opts, rocksdb::Slice(key, key_len), rocksdb::Slice(val, val_len));
    assert(s.ok());
}