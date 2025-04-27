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
    dt->pool_values = NULL;
    hash_value_t *value = NULL;
    pool_get(dt->pool_values, value);
    value->data = format(0, "*%c", 0);
    value->counter = ~0U;
}

u8 **break_domain(char *domain)
{
    u8 **labels = NULL;
    char *token = NULL;
    char *save = NULL;

    token = clib_strtok(domain, LABEL_DLM, &save);
    while (token) {
        vec_add1(labels, format(0, "%s%c", token, 0));
        token = clib_strtok(NULL, LABEL_DLM, &save);
    }

    return labels;
}

u32 *add_label(domain_trie_t *dt, const u8 *label)
{
    hash_value_t *value = 0;

    pool_get(dt->pool_values, value);
    value->counter = 1;
    value->data = format(0, "%v%c", label, 0);

    u32 *tmp = 0;
    vec_add1(tmp, value - dt->pool_values);

    return tmp;
}

u32 *update_label(domain_trie_t *dt, u64 value, const u8 *label)
{
    u32 *idxs = (u32 *)value;
    u32 *idx = 0;
    int found = 0;

    vec_foreach(idx, idxs) {
        if (vec_is_equal(dt->pool_values[*idx].data, label)) {
            dt->pool_values[*idx].counter++;
            found = 1;
            break;
        }
    }

    if (!found) {
        hash_value_t *value = 0;
        pool_get(dt->pool_values, value);
        value->counter = 1;
        value->data = format(0, "%v%c", label, 0);
        vec_add1(idxs, value - dt->pool_values);
    }

    return idxs;
}

void insert_domain_labels(domain_trie_t *dt, u8 ** labels)
{
    char *token = NULL;
    char *save = NULL;
    BVT(clib_bihash_kv) kv = {0};

    for (int i = 0; i < vec_len(labels); ++i) {
        kv.key = clib_crc32c(labels[i], vec_len(labels[i]));

        int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv);
        if (rc < 0) {
            kv.value = pointer_to_u64(add_label(dt, labels[i]));
        }else {
            kv.value = pointer_to_u64(update_label(dt, kv.value,  labels[i]));
        }

        BV(clib_bihash_add_del)(&(dt->labels), &kv, 1);
    }
}

u64 get_label_index(domain_trie_t *dt, const u8 *label)
{
    BVT(clib_bihash_kv) kv;
    u64 ret = ~0ULL;

    kv.key = pointer_to_u64(label);

    int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv);
    if (rc == 0) {
        ret = kv.value;
    }
    return ret;
}

int domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets)
{
    char *copy = strndup(domain, DOMAIN_MAX);
    int rc = 0;
    u8 **labels = break_domain(copy);
    insert_domain_labels(dt, labels);

    BVT(clib_bihash_kv) kv = {0};
    u8 *prefix = 0;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        u64 idx = get_label_index(dt, labels[i]);
        if (idx == ~0ULL) {
            return -1;
        }

        prefix = format(prefix, "%llu.", idx);

        u8 *key = format(0, "%v%c", prefix, 0);
        kv.key = pointer_to_u64(key);

        rc =  BV(clib_bihash_search)(&(dt->trie), &kv, &kv);
        if (rc < 0) {
            kv.value = 1;
            rc = BV(clib_bihash_add_del)(&(dt->trie), &kv, 1);
            if (rc < 0) {
                return ~1;
            }
        } else {
            vec_free(key);
        }

    }

    kv.value = backendsets;
    rc = BV(clib_bihash_add_del)(&(dt->backendsets), &kv, 1);
    if (rc < 0) {
        return ~1;
    }

    free(copy);
    vec_free(labels);
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
        u8 *key = format(0, "%v%c", prefix, 0);
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
        /*vec_free(key);*/
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
