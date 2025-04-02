#include <arm_neon.h>
#include <assert.h>
#include <bits/types/struct_rusage.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "domain_iprtree.h"
#include "domain_trie.h"
#include "vppinfra/format.h"
#include <sys/resource.h>
#include <sys/time.h>

#define count 1000000
#define max_len 253
#define label_min 3
#define label_max 63
#define label_count 4

int dump_hash_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    fformat(stdout, "%llu %llu\n", kv->key, kv->value);
    return 1;
}


void dump_hash_table(BVT(clib_bihash) *ht)
{
    BV(clib_bihash_foreach_key_value_pair)(ht, dump_hash_kv, (void *)0);
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
    clib_mem_init(0, 11ULL << 30);

    domain_trie_init(&dt);

    char (*domains)[count * max_len + 1] = malloc((uint64_t)(count * max_len + 1));

    for (int i = 0; i < count * max_len; i += max_len) {
        generate_domains(&(*domains)[i]);
    }

    if (0) {
        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);

        for (int i = 0; i < count * max_len; i += max_len) {
            domain_trie_insert(&dt, &(*domains)[i], i);
            fformat(stdout, "%s %d\n", &(*domains)[i], i / max_len);
        }

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stdout,"Insertion: time: %llu sec, memory: %llu KB\n", all_time, all_mem);

    /*u64 backendsets;*/

    /*backendsets = domain_trie_search(&dt, &(*domains)[(count -1) * max_len - 0]);*/
    /*fformat(stdout, "%s: %llu\n", &(*domains)[(count -1) * max_len - 0], backendsets);*/
    /*assert(backendsets == (count -1) * max_len - 0);*/

    /*domain_trie_insert(&dt, "def.hg.com", 12);*/
    /*domain_trie_insert(&dt, "abc.def.hg.com", 24);*/

    /*const char *test = "def.hg.com";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 12);*/

    /*test = "abc.def.hg.com";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 24);*/

    /*domain_trie_insert(&dt, "*.acgw.cisco.com", 90);*/
    /*domain_trie_insert(&dt, "1547.*.sc.ciscoplus.com", 200);*/
    /*domain_trie_insert(&dt, "usw1.*.sc.*.cisco.com", 300);*/

    /*test = "123.acgw.cisco.com";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 90);*/

    /*test = "1547.lax.sc.ciscoplus.com";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 200);*/

    /*test = "usw1.lax.sc.zproxy.cisco.com";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 300);*/

    } else {
        // iprtree
        sniproxy_main_t sm = {0};
        domain_iprtree_init(&sm);

        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);


        for (int i = 0; i < count * max_len; i += max_len) {
            domain_iprtree_insert(&sm, &(*domains)[i], i);
            fformat(stdout, "%s %d\n", &(*domains)[i], i / max_len);
        }

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        u64 all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        u64 all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stdout,"Insertion: time: %llu sec, memory: %llu KB\n", all_time, all_mem);

        getrusage(RUSAGE_SELF, &start_res);
        gettimeofday(&start_time, NULL);

        domain_iprtree_commit(&sm);

        getrusage(RUSAGE_SELF, &end_res);
        gettimeofday(&end_time, NULL);

        all_mem = end_res.ru_maxrss - start_res.ru_maxrss;
        all_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000L;
        fformat(stdout,"Build tree: time: %llu sec, memory: %llu KB\n", all_time, all_mem);
    }

    free(domains);
    return EXIT_SUCCESS;
}
