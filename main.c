#include <assert.h>
#include "domain_trie.h"

int main() 
{
    domain_trie_t dt = {0};
    clib_mem_init(0, 126ULL << 20);

    domain_trie_init(&dt);

    domain_trie_insert(&dt, "abc.def.hg.com", 12);
    domain_trie_insert(&dt, "123.def.hg.com", 23);

    domain_trie_insert(&dt, "ag.def.hg.org", 45);
    domain_trie_insert(&dt, "123.def.hg.org", 78);

    domain_trie_insert(&dt, "*.acgw.cisco.com", 90);
    domain_trie_insert(&dt, "1547.*.sc.ciscoplus.com", 200);
    domain_trie_insert(&dt, "usw1.*.sc.*.cisco.com", 300);

    domain_trie_dump(&dt);

    const char *test = "123.def.hg.com";
    u64 backendsets = domain_trie_search(&dt, test);
    fformat(stdout, "%s: %llu\n", test, backendsets);
    assert(backendsets == 23);

    test = "123.def.hg.org";
    backendsets = domain_trie_search(&dt, test);
    fformat(stdout, "%s: %llu\n", test, backendsets);
    assert(backendsets == 78);

    test = "123.acgw.cisco.com";
    backendsets = domain_trie_search(&dt, test);
    fformat(stdout, "%s: %llu\n", test, backendsets);
    assert(backendsets == 90);

    test = "1547.lax.sc.ciscoplus.com";
    backendsets = domain_trie_search(&dt, test);
    fformat(stdout, "%s: %llu\n", test, backendsets);
    assert(backendsets == 200);

    test = "usw1.lax.sc.zproxy.cisco.com";
    backendsets = domain_trie_search(&dt, test);
    fformat(stdout, "%s: %llu\n", test, backendsets);
    assert(backendsets == 300);

    return EXIT_SUCCESS;
}
