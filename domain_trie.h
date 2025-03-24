#ifndef DOMAIN_TRIE_H
#define DOMAIN_TRIE_H

#include <unistd.h>
#include <vppinfra/clib.h>
#include <vppinfra/crc32.h>
#include <vppinfra/format.h>
#include <vppinfra/mem.h>
#include <vppinfra/string.h>
#include <vppinfra/types.h>
#include <vppinfra/pool.h>
#include <vppinfra/vec.h>
#include <vppinfra/bihash_8_8.h>
#include <vppinfra/bihash_template.h>

#define DOMAIN_MAX 253
#define LABEL_MAX 63
#define LABEL_DLM "."
#define TRIE_HASH_SIZE 1024ULL << 20
#define TRIE_HASH_BUCKET 81920
#define TRIE_HASH_NAME "domain_trie_ht"

typedef struct  {
    BVT(clib_bihash) trie;
    BVT(clib_bihash) labels;
    BVT(clib_bihash) backendsets;
    char **pool_label_t;
} domain_trie_t;

void domain_trie_init(domain_trie_t *dt);
void domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets);
u64 domain_trie_search(domain_trie_t *dt, const char *domain);

#endif
