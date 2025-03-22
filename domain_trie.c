#include "domain_trie.h"
#include "vppinfra/pool.h"
#include "vppinfra/types.h"
#include <vppinfra/bihash_template.c>

int dump_hash_kv(BVT(clib_bihash_kv) *kv, void *args)
{
    if (kv->value != 0) {
        trie_node_t *pool = (trie_node_t *)args;
        trie_node_t *node = pool_elt_at_index(pool, kv->value);
        dump_trie_node(node, pool);
    }

    return 1;
}

int count_keys(BVT(clib_bihash_kv) *kv, void *args)
{
    u64 *count = (u64 *)args;
    ++(*count);
    return 1;
}

void dump_hash_table(BVT(clib_bihash) *ht, trie_node_t *pool)
{
    BV(clib_bihash_foreach_key_value_pair)(ht, dump_hash_kv, pool);
}

void dump_trie_node(trie_node_t *node, trie_node_t *pool)
{
    u64 count = 0;
    BV(clib_bihash_foreach_key_value_pair)(&node->ht, count_keys, &count);
    //fformat(stdout, "label: %s, number of keys: %llu, backendsets: %llu\n", node->label, count , node->backendsets);
    dump_hash_table(&node->ht, pool);
}

void domain_trie_dump(domain_trie_t *dt)
{
    trie_node_t *root = &dt->trie_pool_start[0];
    dump_trie_node(root, dt->trie_pool_start);
}

void domain_trie_init(domain_trie_t *dt)
{
    pool_get(dt->trie_pool_start, dt->trie_pool_cur);
    BV(clib_bihash_init)(&(dt->trie_pool_cur->ht), TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
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

void domain_trie_insert(domain_trie_t *dt, const char *domain, u64 backendsets)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    trie_node_t **pool = &dt->trie_pool_start;
    trie_node_t **cur = &dt->trie_pool_cur;
    u64 current_index = pool_get_first_index(*pool);
    trie_node_t *current = NULL;

    for (int i = vec_len(labels); i > 0; i--) {
        kv.key = (u64)format(0, "%s", labels[i - 1]);
        kv.value = CLIB_U64_MAX;

        current = pool_elt_at_index(*pool, current_index);
        int rc = BV(clib_bihash_search)(&(current->ht), &kv, &kv);

        if (rc < 0) {
            pool_get(*pool, *cur);
            //(*cur)->label = format(0, "%s", labels[i -1]);
            (*cur)->backendsets = CLIB_U64_MAX;
            BV(clib_bihash_init)(&(*cur)->ht, TRIE_HASH_NAME, TRIE_HASH_BUCKET, TRIE_HASH_SIZE);
            kv.value = *cur - *pool;

            if (i == 1) {
                (*cur)->backendsets = backendsets;
            }

            current = pool_elt_at_index(*pool, current_index);
            BV(clib_bihash_add_del)(&current->ht, &kv, 1);
            current_index = *cur - *pool;
        } else {
             current_index = pool_get_next_index(&dt->trie_pool_start, current_index);
        }

    }

    free(copy);
    vec_free(labels);
}

u64 domain_trie_search(domain_trie_t *dt, const char *domain)
{
    char *copy = strndup(domain, LABEL_MAX);
    char **labels = break_domain(copy);

    BVT(clib_bihash_kv) kv = {0};
    trie_node_t **pool = &dt->trie_pool_start;
    trie_node_t **cur = &dt->trie_pool_cur;
    u64 current_index = pool_get_first_index(*pool);
    trie_node_t *current = pool_elt_at_index(*pool, current_index);
    u64 best_match = current->backendsets;

    for (int i = vec_len(labels); i > 0; i--) {
        kv.key = (u64)format(0, "%s", labels[i - 1]);
        kv.value = 0;

        int rc = BV(clib_bihash_search)(&(current->ht), &kv, &kv);

        if (rc < 0) {
            kv.key = (u64)format(0, "%s", "*");
            kv.value = 0;

            int rc = BV(clib_bihash_search)(&(current->ht), &kv, &kv);
            if (rc < 0) {
                break;
            } else {
                current_index = kv.value;
                current = pool_elt_at_index(*pool, current_index);
                best_match = current->backendsets;
            }
        } else {
            current_index = kv.value;
            current = pool_elt_at_index(*pool, current_index);
            best_match = current->backendsets;
        }

    }

    free(copy);
    vec_free(labels);
    return best_match;
}
