#include <assert.h>
#include <sys/time.h>
#include "domain_iprtree.h"
#include "domain_trie.h"
#include "vppinfra/format.h"

#define count 300000
#define max_len 253
#define label_min 3
#define label_max 63
#define label_count 4

int dump_hash_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    fformat(stderr, "%s %llu\n", (u8 *)kv->key, kv->value);
    return 1;
}

void dump_hash_table(BVT(clib_bihash) *ht)
{
    BV(clib_bihash_foreach_key_value_pair)(ht, dump_hash_kv, (void *)0);
}

void debug_hash_table(domain_trie_t *dt)
{
    fformat(stderr, "\n\ndump trie\n");
    dump_hash_table(&dt->trie);

    fformat(stderr, "\n\ndump backend\n");
    dump_hash_table(&dt->backendsets);

    fformat(stderr, "\n\ndump labels\n");
    dump_hash_table(&dt->labels);

    fflush(stderr);
    fflush(stderr);
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

int main()
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

        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"\nInsertion: time: %llu sec, memory: %llu KB\n\n", all_time, all_mem);


        gettimeofday(&start_time, NULL);
        for (int i = 0; i < count * max_len; i += max_len) {
            u64 backendsets = domain_trie_search(&dt, &(*domains)[i]);
            assert(backendsets == (i / max_len));
        }
        gettimeofday(&end_time, NULL);

        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"\nLookup: time: %llu sec\n", all_time);

    } else {
        // iprtree
        sniproxy_main_t sm = {0};
        domain_iprtree_init(&sm);


        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);


        for (int i = 0; i < count * max_len; i += max_len) {
            u8 *pattern = format(0, "*.%s", &(*domains)[i]);
            domain_iprtree_insert(&sm, (const char *)pattern, i / max_len);
            /*fformat(stderr, "insert: %s %d\n", (const char *)pattern, i / max_len);*/
        }

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"Insertion: time: %llu sec, memory: %llu KB\n", all_time, all_mem);

        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);

        domain_iprtree_commit(&sm);

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"Build tree: time: %llu sec, memory: %llu KB\n", all_time, all_mem);

        gettimeofday(&start_time, NULL);
        for (int i = 0; i < count * max_len; i += max_len) {
            u8 *pattern = format(0, "1.%s", &(*domains)[i]);
            u64 backendsets = domain_iprtree_search(&sm, (const char*)pattern);
            /*fformat(stderr, "%s: %llu\n", pattern, backendsets );*/
            /*assert(backendsets == i / max_len);*/
            if (backendsets != (i / max_len)) {
                backendsets = domain_iprtree_search(&sm, (const char*)pattern);
            }
        }
        gettimeofday(&end_time, NULL);

        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stderr,"Lookup: time: %llu sec\n", all_time);



        /*const char *test = "vhqbl5fxb22ts13zes5j66a6l7sayn0yu.d5pi7qac-shgvmcp-8la.al0pocebazekd2vu2x.y4djufcud29l2jpvx70clowv7nfqtg";*/
        /*domain_iprtree_insert(&sm, test, 0);*/
        /*domain_iprtree_commit(&sm);*/
        /*u64 backendsets;*/
        /*backendsets = domain_iprtree_search(&sm, "vhqbl5fxb22ts13zes5j66a6l7sayn0yu.d5pi7qac-shgvmcp-8la.al0pocebazekd2vu2x.y4djufcud29l2jpvx70clowv7nfqtg");*/
        /*fformat(stderr, "%s: %llu\n", test, backendsets);*/
        /*assert(backendsets == 0);*/
    }

    free(domains);
    return EXIT_SUCCESS;
}
