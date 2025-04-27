#include "domain_trie.h"
#include "vppinfra/pool.h"
#include "vppinfra/string.h"
#include "vppinfra/vec.h"
#include "vppinfra/vec_bootstrap.h"
#include <vppinfra/bihash_template.c>

void domain_trie_init(domain_trie_t *dt)
{
    BV(clib_bihash_init)(&(dt->trie), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->labels), LABEL_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->backendsets), BACK_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    dt->pool_trie = NULL;
    dt->pool_backendsets = NULL;
    dt->pool_labels = NULL;
}

static u8 **break_domain(char *domain)
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

static u32 *add_label(domain_trie_t *dt, const u8 *label)
{
    hash_value_t *value = 0;

    pool_get(dt->pool_labels, value);
    value->counter = 1;
    value->data = format(0, "%v%c", label, 0);

    u32 *tmp = 0;
    vec_add1(tmp, value - dt->pool_labels);

    return tmp;
}

static u32 *update_label(domain_trie_t *dt, u64 value, const u8 *label)
{
    u32 *idxs = (u32 *)value;
    u32 *idx = 0;
    int found = 0;

    vec_foreach(idx, idxs) {
        if (vec_is_equal(dt->pool_labels[*idx].data, label)) {
            dt->pool_labels[*idx].counter++;
            found = 1;
            break;
        }
    }

    if (!found) {
        hash_value_t *value = 0;
        pool_get(dt->pool_labels, value);
        value->counter = 1;
        value->data = format(0, "%v%c", label, 0);
        vec_add1(idxs, value - dt->pool_labels);
    }

    return idxs;
}

static void insert_domain_labels(domain_trie_t *dt, u8 ** labels)
{
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

static u64 get_label_index(domain_trie_t *dt, const u8 *label)
{
    BVT(clib_bihash_kv) kv;
    u64 ret = ~0ULL;

    u8 *tmp = format(0, "%s%c", label, 0);

    kv.key = clib_crc32c(tmp, vec_len(tmp));

    int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv);
    if (rc == 0) {
        u32 *idxs =(u32 *)kv.value;
        u32 *idx = 0;
        vec_foreach(idx, idxs) {
            if (vec_is_equal(pool_elt_at_index(dt->pool_labels, *idx)->data, label)) {
                return *idx;
            }
        }
    }
    return ret;
}

static u32 *add_trie(domain_trie_t *dt, const u8 *label)
{
    hash_value_t *value = 0;

    pool_get(dt->pool_trie, value);
    value->counter = 1;
    value->data = format(0, "%v%c", label, 0);

    u32 *tmp = 0;
    vec_add1(tmp, value - dt->pool_trie);

    return tmp;
}

static u32 *update_trie(domain_trie_t *dt, u64 value, const u8 *label)
{
    u32 *idxs = (u32 *)value;
    u32 *idx = 0;
    int found = 0;

    vec_foreach(idx, idxs) {
        if (vec_is_equal(dt->pool_trie[*idx].data, label)) {
            dt->pool_trie[*idx].counter++;
            found = 1;
            break;
        }
    }

    if (!found) {
        hash_value_t *value = 0;
        pool_get(dt->pool_trie, value);
        value->counter = 1;
        value->data = format(0, "%v%c", label, 0);
        vec_add1(idxs, value - dt->pool_trie);
    }

    return idxs;
}

static u32 *add_backendset(domain_trie_t *dt, const u8 *suffix, u64 backendsets)
{
    hash_value_t *value = 0;

    pool_get(dt->pool_backendsets, value);
    value->backendsets = backendsets;
    value->data = format(0, "%v%c", suffix, 0);

    u32 *tmp = 0;
    vec_add1(tmp, value - dt->pool_backendsets);

    return tmp;
}

static u32 *update_backendset(domain_trie_t *dt, u64 value, const u8 *suffix, u64 backendsets)
{
    u32 *idxs = (u32 *)value;
    u32 *idx = 0;
    int found = 0;

    vec_foreach(idx, idxs) {
        if (vec_is_equal(dt->pool_backendsets[*idx].data, suffix)) {
            dt->pool_backendsets[*idx].backendsets = backendsets;
            found = 1;
            break;
        }
    }

    if (!found) {
        hash_value_t *value = 0;
        pool_get(dt->pool_backendsets, value);
        value->backendsets = backendsets;
        value->data = format(0, "%v%c", suffix, 0);
        vec_add1(idxs, value - dt->pool_backendsets);
    }

    return idxs;
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
        kv.key = clib_crc32c(prefix, vec_len(prefix));

        rc =  BV(clib_bihash_search)(&(dt->trie), &kv, &kv);
        if (rc < 0) {
            kv.value = pointer_to_u64(add_trie(dt, labels[i]));
        } else {
            kv.value = pointer_to_u64(update_trie(dt, kv.value,  labels[i]));
        }

        BV(clib_bihash_add_del)(&(dt->labels), &kv, 1);
    }

    rc =  BV(clib_bihash_search)(&(dt->backendsets), &kv, &kv);
    if (rc < 0) {
        kv.value = pointer_to_u64(add_backendset(dt, prefix, backendsets));
    } else {
        kv.value = pointer_to_u64(update_backendset(dt, kv.value, prefix, backendsets));
    }
    BV(clib_bihash_add_del)(&(dt->backendsets), &kv, 1);

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
        kv.key = clib_crc32c(prefix, vec_len(prefix));

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            vec_del1(prefix, vec_len(prefix) - 1);
            prefix = format(prefix, "*%c", 0);
            kv.key = clib_crc32c(prefix, vec_len(prefix));

            int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
            if (rc < 0) {
                break;
            } else {
                best_match_key = kv.key;
            }
        } else {
            best_match_key = kv.key;
        }
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
