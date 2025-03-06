#include <arm_neon.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "domain_trie.h"
#include "vppinfra/bihash_template.h"

#define count 30000
#define max_len 24
#define label_min 3
#define label_max 5
#define label_count 4


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
    srand(arc4random());
    domain_trie_t dt = {0};
    clib_mem_init(0, 11ULL << 30);

    domain_trie_init(&dt);

    char (*domains)[count * max_len + 1] = malloc((uint64_t)count * (max_len + 1));

    for (int i = 0; i < count * max_len; i += max_len) {
        generate_domains(&(*domains)[i]);
        domain_trie_insert(&dt, &(*domains)[i], i);
        fformat(stdout, "%s %d\n", &(*domains)[i], i / max_len);
    }

    /*domain_trie_insert(&dt, "abc.def.hg.com", 12);*/
    /*domain_trie_insert(&dt, "123.def.hg.com", 23);*/

    /*domain_trie_insert(&dt, "ag.def.hg.org", 45);*/
    /*domain_trie_insert(&dt, "123.def.hg.org", 78);*/

    /*domain_trie_insert(&dt, "*.acgw.cisco.com", 90);*/
    /*domain_trie_insert(&dt, "1547.*.sc.ciscoplus.com", 200);*/
    /*domain_trie_insert(&dt, "usw1.*.sc.*.cisco.com", 300);*/

    /*domain_trie_dump(&dt);*/


    int idx = 10 * max_len;
    u64 backendsets = domain_trie_search(&dt, &(*domains)[idx]);
    fformat(stdout, "%s: %llu\n", &(*domains)[idx], backendsets);
    assert(backendsets == ((u64)idx));

    /*test = "123.def.hg.org";*/
    /*backendsets = domain_trie_search(&dt, test);*/
    /*fformat(stdout, "%s: %llu\n", test, backendsets);*/
    /*assert(backendsets == 78);*/

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

    return EXIT_SUCCESS;
}
