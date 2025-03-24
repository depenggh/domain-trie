#include "domain_trie.h"
#include "vppinfra/string.h"
#include "vppinfra/vec.h"
#include <vppinfra/bihash_template.c>

void domain_trie_init(domain_trie_t *dt)
{
    BV(clib_bihash_init)(&(dt->trie), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->labels), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->backendsets), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    dt->pool_label_t = NULL;
}

char **break_domain(char *domain)
{
    char **labels = NULL;
    char *token = NULL;
    char *save = NULL;

    token = clib_strtok(domain, LABEL_DLM, &save);
    while (token) {
        vec_add1(labels, token);
        token = clib_strtok(NULL, LABEL_DLM, &save);
    }

    return labels;
}

u64 get_label_index(domain_trie_t *dt, char *label)
{
    BVT(clib_bihash_kv) kv = {0};
    kv.key = clib_crc32c((const u8*)label, clib_strnlen(label, LABEL_MAX));

    int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
    if (rc < 0) {
        vec_add1(dt->pool_label_t, label);
        kv.value = vec_len(dt->pool_label_t) - 1;
        BV(clib_bihash_add_del)(&dt->trie, &kv, 1);
    }

    return kv.value;
}

void domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    u8 *prefix = NULL;
    u64 prefix_hash;
    u64 label_hash;

    for (int i = vec_len(labels) - 1; i >= 0; --i) {
        prefix = format(prefix, "%s", labels[i]);
        prefix_hash = clib_crc32c(prefix, clib_strnlen((const char*)prefix, LABEL_MAX));

        kv.key = prefix_hash;

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            kv.value = get_label_index(dt, labels[i]);
            BV(clib_bihash_add_del)(&dt->trie, &kv, 1);
        }
    }

    kv.value = backendsets;
    int rc = BV(clib_bihash_add_del)(&dt->backendsets, &kv, 1);

    free(copy);
    vec_free(labels);
}


u64 domain_trie_search(domain_trie_t *dt, const char *domain)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    u64 best_match = CLIB_U64_MAX;
    u8 *prefix = NULL;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        prefix = format(prefix, "%s", labels[i]);
        kv.key = clib_crc32c(prefix, clib_strnlen((const char*)prefix, LABEL_MAX));

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            vec_del1(prefix, vec_len(prefix) - 1);
            prefix = format(prefix, "*");
            kv.key = clib_crc32c(prefix, clib_strnlen((const char*)prefix, LABEL_MAX));

            int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
            if (rc < 0) {
                break;
            }
        }
    }

    int rc = BV(clib_bihash_search)(&(dt->backendsets), &kv, &kv );
    if (rc >= 0) {
        best_match = kv.value;
    }

    free(copy);
    vec_free(labels);
    return best_match;
}
