#include "domain_trie.h"
#include <vppinfra/string.h>
#include <vppinfra/vec.h>
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
    u8 *key = format(0, "%s%c", label, '\0');
    kv.key = (u64)key;

    int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv );
    if (rc < 0) {
        u8 *tmp = vec_dup(key);
        vec_add1(dt->pool_label_t, (char *)tmp);
        kv.key = (u64)tmp;
        kv.value = vec_len(dt->pool_label_t) - 1;
        BV(clib_bihash_add_del)(&dt->labels, &kv, 1);
    }

    vec_free(key);
    return kv.value;
}

int domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    u8 *prefix = NULL;

    for (int i = vec_len(labels) - 1; i >= 0; --i) {
        u64 idx = get_label_index(dt, labels[i]);
        if (idx == ~(u64)0) {
            return -1;
        }

        prefix = format(prefix, "%llu ", idx);
        u8 *key = format(0, "%v%c", prefix, '\0');
        kv.key = (u64)key;

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            kv.key = (u64)vec_dup(key);
            kv.value = 1;
            BV(clib_bihash_add_del)(&dt->trie, &kv, 1);
        }

        if (i == 0) {
            kv.value = backendsets;
            BV(clib_bihash_add_del)(&dt->backendsets, &kv, 1);
        }
        vec_free(key);
    }

    free(copy);
    vec_free(labels);
    vec_free(prefix);
    return 0;
}


u64 domain_trie_search(domain_trie_t *dt, const char *domain)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    u64 best_match_key = CLIB_U64_MAX;
    u64 best_match = CLIB_U64_MAX;
    u8 *prefix = NULL;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        u64 idx = get_label_index(dt, labels[i]);
        if (idx == ~(u64)0) {
            return -1;
        }

        prefix = format(prefix, "%llu ", idx);
        u8 *key = format(0, "%v%c", prefix, '\0');
        kv.key = (u64)key;

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            vec_del1(key, vec_len(key) - 1);
            vec_add1(key, 0);
            kv.key = (u64)key;

            int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
            if (rc < 0) {
                break;
            } else {
                best_match_key = kv.key;
            }
        } else {
            best_match_key = kv.key;
        }
        vec_free(key);
    }

    kv.key = best_match_key;
    int rc = BV(clib_bihash_search)(&(dt->backendsets), &kv, &kv );
    if (rc >= 0) {
        best_match = kv.value;
    }

    free(copy);
    vec_free(labels);
    vec_free(prefix);
    return best_match;
}
