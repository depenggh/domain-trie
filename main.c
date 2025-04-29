#include <assert.h>
#include <stdio.h>
#include <sys/time.h>
#include "domain_iprtree.h"
#include "domain_trie.h"
#include "vppinfra/format.h"
#include "vppinfra/vec_bootstrap.h"

#define count 1000000
#define max_len 253
#define label_min 3
#define label_max 63
#define label_count 4

int count_kvs(BVT(clib_bihash_kv) *kv, void *args)
{
    u64 *a = args;
    u32 *idxs = (u32 *)kv->value;
    u32 *idx = 0;
    vec_foreach(idx, idxs) {
        ++*a;
    }

    return 1;
}

u64 count_kv_table(BVT(clib_bihash) *ht)
{
    u64 a = 0;

    BV(clib_bihash_foreach_key_value_pair)(ht, count_kvs, (void *)&a);

    return a;
}

int dump_labels_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    domain_trie_t *dt = args;
    u32 *idxs = (u32 *)kv->value;
    u32 *idx = 0;
    vec_foreach(idx, idxs) {
        hash_value_t *value = &dt->pool_labels[*idx];
        fformat(stderr, "%llu %v %llu", kv->key, value->data, value->counter);
    }
    fformat(stderr, "\n\n");
    return 1;
}

void dump_labels_table(domain_trie_t *dt)
{
    BV(clib_bihash_foreach_key_value_pair)(&dt->labels, dump_labels_kv, (void *)dt);
}

int dump_back_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    domain_trie_t *dt = args;
    u32 *idxs = (u32 *)kv->value;
    u32 *idx = 0;
    vec_foreach(idx, idxs) {
        hash_value_t *value = &dt->pool_backendsets[*idx];
        fformat(stderr, "%llu %v %llu", kv->key, value->data, value->backendsets);
    }
    fformat(stderr, "\n\n");
    return 1;
}

void dump_back_table(domain_trie_t *dt)
{
    BV(clib_bihash_foreach_key_value_pair)(&dt->backendsets, dump_back_kv, (void *)dt);
}

int dump_trie_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    domain_trie_t *dt = args;
    u32 *idxs = (u32 *)kv->value;
    u32 *idx = 0;
    vec_foreach(idx, idxs) {
        hash_value_t *value = &dt->pool_trie[*idx];
        fformat(stderr, "%llu %v %llu", kv->key, value->data, value->counter);
    }
    fformat(stderr, "\n\n");
    return 1;
}

void dump_trie_table(domain_trie_t *dt)
{
    BV(clib_bihash_foreach_key_value_pair)(&dt->trie, dump_trie_kv, (void *)dt);
}

void generate_domains(char *domain)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789-";
    int pos = 0;
    for (int i = 0; i < label_count; i++) {
        int label_len = label_min + rand() % (label_max - label_min);
        for (int j = 0; j < label_len; j++) {
            domain[pos++] = charset[rand() % (sizeof(charset) - 1)];
        }

        if (i < label_count - 1) {
            domain[pos++] = '.';
        }
    }
    domain[pos] = '\0';
}

int run()
{
    struct rusage start_res, end_res;
    struct timeval start_time, end_time;
    srand(arc4random());
    domain_trie_t dt = {0};
    clib_mem_init(0, 8ULL << 30);

    domain_trie_init(&dt);

    char (*domains)[count * max_len + 1] = calloc(count * max_len + 1, sizeof(char));

    for (int i = 0; i < count * max_len; i += max_len) {
        generate_domains(&(*domains)[i]);
    }

    /*FILE *file = fopen("data.txt", "w");*/

    /*size_t rc = fwrite((*domains), sizeof(char), count * max_len + 1, file);*/
    /*exit(0);*/

    /*FILE *file = fopen("data.txt", "r");*/
    /*size_t rc = fread(&(*domains), sizeof(char), count * max_len + 1, file);*/
    /*fclose(file);*/

    if (1) {
        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);

        for (int i = 0; i < count * max_len; i += max_len) {
            int rc = domain_trie_insert(&dt, &(*domains)[i], i / max_len);
            assert(rc == 0);
        }

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        fformat(stderr, "Patricia Trie:\n");
        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"inserting %llu patterns: time: %llu sec, memory: %llu KB\n\n", count, all_time, all_mem);

        gettimeofday(&start_time, NULL);
        for (int i = 0; i < count * max_len; i += max_len) {
            u64 backendsets = domain_trie_search(&dt, &(*domains)[i]);
            assert(backendsets == (i / max_len));
        }
        gettimeofday(&end_time, NULL);

        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"sarching %llu patterns: time: %llu sec\n", count, all_time);

        u64 k = count_kv_table(&dt.backendsets);
        assert(k == count);

        int rc = domain_trie_insert(&dt, "*.cisco.io", 12);
        assert(rc == 0);

        u64 backendsets = domain_trie_search(&dt, "1.cisco.io");
        assert(backendsets == 12);

    } else {
        // iprtree
        sniproxy_main_t sm = {0};
        domain_iprtree_init(&sm);


        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);


        for (int i = 0; i < count * max_len; i += max_len) {
            u8 *pattern = format(0, "*.%s", &(*domains)[i]);
            domain_iprtree_insert(&sm, (const char *)pattern, i / max_len);
        }

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        fformat(stderr, "Iprtree:\n");
        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"inserting %llu patterns: time: %llu sec, memory: %llu KB\n", count, all_time, all_mem);

        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);

        domain_iprtree_commit(&sm);

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"building tree for %llu patterns: time: %llu sec, memory: %llu KB\n", count, all_time, all_mem);

        gettimeofday(&start_time, NULL);
        int i = 0;
        for (i = 0; i < count * max_len; i += max_len) {
            u8 *pattern = format(0, "1.%s", &(*domains)[i]);
            u64 backendsets = domain_iprtree_search(&sm, (const char*)pattern);
            assert(backendsets == i / max_len);
        }
        gettimeofday(&end_time, NULL);

        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"searching %llu patterns: time: %llu sec\n", count, all_time);

    }

    free(domains);
    return EXIT_SUCCESS;
}

int main()
{
    return run();
}
