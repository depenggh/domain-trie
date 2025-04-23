#include "domain_trie.h"
#include "vppinfra/string.h"
#include "vppinfra/vec.h"
#include "vppinfra/vec_bootstrap.h"
#include <vppinfra/bihash_template.c>

void domain_trie_init(domain_trie_t *dt)
{
    BV(clib_bihash_init)(&(dt->trie), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->labels), LABEL_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->backendsets), BACK_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    dt->vec_label_t = NULL;
    vec_add1(dt->vec_label_t, format(0, "*"));
}

void insert_domain_labels(domain_trie_t *dt, u32 *start, u32 *end, char *domain)
{
    char *token = NULL;
    char *save = NULL;
    BVT(clib_bihash_kv) kv = {0};
    BVT(clib_bihash_kv) valuep = {0};

    *start = vec_len(dt->vec_label_t) - 1;

    token = clib_strtok(domain, LABEL_DLM, &save);
    while (token) {
        kv.key = (u64)format(0, "%s", token);
        int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &valuep);
        if (rc < 0) {
            vec_add1(dt->vec_label_t, (u8 *)kv.key);
            kv.value = vec_len(dt->vec_label_t) - 1;
            BV(clib_bihash_add_del)(&(dt->labels), &kv, 1);
        } else {
            /*u8 *existing_key = (u8 *)kv.key;*/
            /*if (existing_key != (u8*)valuep.key) {*/
                /*vec_free(existing_key);*/
            /*}*/
        }
        token = clib_strtok(NULL, LABEL_DLM, &save);
    }
    *end = vec_len(dt->vec_label_t) - 1;
}

u8 **break_domain(char *domain)
{
    u8 **labels = NULL;
    char *token = NULL;
    char *save = NULL;

    token = clib_strtok(domain, LABEL_DLM, &save);
    while (token) {
        vec_add1(labels, format(0, "%s", token));
        token = clib_strtok(NULL, LABEL_DLM, &save);
    }

    return labels;
}

u64 get_label_index(domain_trie_t *dt, const u8 *label)
{
    BVT(clib_bihash_kv) kv;
    u64 ret = ~0ULL;

    u8 *key = vec_dup(label);
    kv.key = pointer_to_u64(key);

    int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv);
    if (rc == 0) {
        ret = kv.value;
    }
    return ret;
}

int domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets)
{
    char *copy = strndup(domain, DOMAIN_MAX);
    u32 start = 0;
    u32 end = 0;
    insert_domain_labels(dt, &start, &end, copy);

    BVT(clib_bihash_kv) kv = {0};
    u8 *prefix = 0;

    for (u32 i = end; i > start; --i) {
        u64 idx = get_label_index(dt, dt->vec_label_t[i]);
        if (idx == ~0ULL) {
            return -1;
        }

        prefix = format(prefix, "%llu.", idx);

        u8 *key = format(0, "%v%c", prefix, 0);
        kv.key = pointer_to_u64(key);

        int rc =  BV(clib_bihash_search)(&(dt->trie), &kv, &kv);
        if (rc < 0) {
            kv.value = 1;
            rc = BV(clib_bihash_add_del)(&(dt->trie), &kv, 1);
            if (rc < 0) {
                return ~1;
            }
        }

        if (i == 0) {
            kv.value = backendsets;
            rc = BV(clib_bihash_add_del)(&(dt->backendsets), &kv, 1);
            if (rc < 0) {
                return ~1;
            }
        }
        /*vec_free(key);*/
    }

    /*free(copy);*/
   /*vec_free(labels);*/
    /*vec_free(prefix);*/
    return 0;
}


u64 domain_trie_search(domain_trie_t *dt, const char *domain)
{
    char *copy = strndup(domain, DOMAIN_MAX);
    u8 **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    u64 best_match_key = ~0ULL;
    u64 best_match = ~0ULL;
    u8 *prefix = NULL;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        u64 idx = get_label_index(dt, labels[i]);
        if (idx == ~0ULL) {
            return -1;
        }

        prefix = format(prefix, "%llu.", idx);
        u8 *key = format(0, "%v%c", prefix, '\0');
        kv.key = pointer_to_u64(key);

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
