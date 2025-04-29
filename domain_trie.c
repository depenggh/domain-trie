#include "domain_trie.h"
#include "vppinfra/pool.h"
#include "vppinfra/string.h"
#include "vppinfra/vec.h"
#include "vppinfra/vec_bootstrap.h"
#include <vppinfra/bihash_template.c>

static void insert_domain_labels(domain_trie_t *dt, u8 ** labels);

void domain_trie_init(domain_trie_t *dt)
{
    BV(clib_bihash_init)(&(dt->trie), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->labels), LABEL_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    BV(clib_bihash_init)(&(dt->backendsets), BACK_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
    dt->pool_trie = NULL;
    dt->pool_backendsets = NULL;
    dt->pool_labels = NULL;
    u8 **labels = 0;
    vec_add1(labels, format(0, "*"));
    insert_domain_labels(dt, labels);
    vec_free(labels);
}

static u8 **break_domain(char *domain)
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

static u32 *add_value(hash_value_t **pool, update_field_t field, const u8 *label, u64 change)
{
    hash_value_t *value = 0;

    pool_get(*pool, value);
    switch (field) {
        case UPDATE_COUNTER:
            value->counter = change;
            break;
        case UPDATE_BACKENSETS:
            value->backendsets = change;
            break;
    }

    value->data = format(0, "%v", label);

    u32 *tmp = 0;
    vec_add1(tmp, value - *pool);

    return tmp;
}

static u32 *update_value(hash_value_t **pool, update_field_t field, u64 value, const u8 *label, u64 change)
{
    u32 *idxs = (u32 *)value;
    u32 *idx = 0;
    int found = 0;

    vec_foreach(idx, idxs) {
        if (vec_is_equal((*pool)[*idx].data, label)) {
            switch (field) {
                case UPDATE_COUNTER:
                    (*pool)[*idx].counter += change;
                    break;
                case UPDATE_BACKENSETS:
                    (*pool)[*idx].backendsets = change;
                    break;
            }

            found = 1;
            break;
        }
    }

    if (!found) {
        hash_value_t *value = 0;
        pool_get(*pool, value);
        switch (field) {
            case UPDATE_COUNTER:
                value->counter = change;
                break;
            case UPDATE_BACKENSETS:
                value->backendsets = change;
                break;
        }

        value->data = format(0, "%v", label);
        vec_add1(idxs, value - *pool);
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
            kv.value = pointer_to_u64(add_value(&dt->pool_labels, UPDATE_COUNTER ,labels[i], 1));
        }else {
            kv.value = pointer_to_u64(update_value(&dt->pool_labels, UPDATE_COUNTER,  kv.value, labels[i], 1));
        }

        BV(clib_bihash_add_del)(&(dt->labels), &kv, 1);
    }
}

static u32 get_label_index(domain_trie_t *dt, const u8 *label)
{
    BVT(clib_bihash_kv) kv;
    u32 ret = ~0U;

    kv.key = clib_crc32c(label, vec_len(label));

    int rc = BV(clib_bihash_search)(&(dt->labels), &kv, &kv);
    if (rc == 0) {
        u32 *idxs =(u32 *)kv.value;
        u32 *idx = 0;
        vec_foreach(idx, idxs) {
            hash_value_t *v = pool_elt_at_index(dt->pool_labels, *idx);
            if (vec_is_equal(v->data, label)) {
                ret = *idx;
            }
        }
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
    u8 *suffix = 0;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        u32 idx = get_label_index(dt, labels[i]);

        suffix = format(suffix, "%llu.", idx);
        kv.key = clib_crc32c(suffix, vec_len(suffix));

        rc =  BV(clib_bihash_search)(&(dt->trie), &kv, &kv);
        if (rc < 0) {
            kv.value = pointer_to_u64(add_value(&dt->pool_trie, UPDATE_COUNTER, suffix, 1));
        } else {
            kv.value = pointer_to_u64(update_value(&dt->pool_trie, UPDATE_COUNTER, kv.value, suffix, 1));
        }

        BV(clib_bihash_add_del)(&(dt->trie), &kv, 1);
    }

    rc =  BV(clib_bihash_search)(&(dt->backendsets), &kv, &kv);
    if (rc < 0) {
        kv.value = pointer_to_u64(add_value(&dt->pool_backendsets,UPDATE_BACKENSETS, suffix, backendsets));
    } else {
        kv.value = pointer_to_u64(update_value(&dt->pool_backendsets, UPDATE_BACKENSETS, kv.value, suffix, backendsets));
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
    u8 *suffix = NULL;

    for (int i = vec_len(labels) - 1; i >= 0; i--) {
        u32 idx = get_label_index(dt, labels[i]);

        u32 old_len = vec_len(suffix);
        suffix = format(suffix, "%llu.", idx);
        kv.key = clib_crc32c(suffix, vec_len(suffix));

        int rc = BV(clib_bihash_search)(&(dt->trie), &kv, &kv );
        if (rc < 0) {
            vec_set_len(suffix, old_len);
            suffix = format(suffix, "%llu.", 0);
            kv.key = clib_crc32c(suffix, vec_len(suffix));

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
    if (rc == 0) {
        u32 *idxs = (u32 *)kv.value;
        u32 *idx = 0;
        vec_foreach(idx, idxs) {
            hash_value_t *tmp = pool_elt_at_index(dt->pool_backendsets, *idx);
            if (vec_is_equal(tmp->data, suffix)){
                best_match = tmp->backendsets;
            }
        }
    }

    free(copy);
    vec_free(labels);
    vec_free(suffix);
    return best_match;
}
