#include "domain_iprtree.h"
#include "sniproxy.h"
#include "vppinfra/format.h"
#include <stdio.h>

void do_init();

u8 *
sniproxy_prepare_pattern (u8 *pattern)
{
  u32 len;
  /* Trim null terminator off, if any */
  if (vec_end (pattern)[-1] == 0)
    vec_dec_len (pattern, 1);

  len = vec_len (pattern);

  /* if it is not a wildcard expression, add terminator at the beginning */
  if (pattern[0] != '*')
    {
      vec_resize (pattern, 1);
      clib_memmove (pattern + 1, pattern, len);
      pattern[0] = 0;
    }
  else
    {
      /* Trim the wildcard character, it is useless */
      clib_memmove (pattern, pattern + 1, len);
      vec_dec_len (pattern, 1);
    }
  return pattern;
}

void
sniproxy_table_rebuild (sniproxy_main_t *sm, sniproxy_table_t *table)
{
  iprtree_container_t *container = &sm->iprtree_container;
  iprtree_t *tree = &table->tree;
  sniproxy_pattern_t *pattern;
  u32 *pattern_index;

  /* Empty the tree */
  iprtree_clear (container, tree);

  /* Create root node */
  tree->iprtree_root_node_index = iprtree_allocate_internal_node (container);

  vec_foreach (pattern_index, table->pattern_indices)
    {
      pattern = sniproxy_pattern_get (sm, pattern_index[0]);
      iprtree_insert_pattern (container, tree, pattern->str,
			      pattern->backend_set_index);
    }
};


void domain_iprtree_init(sniproxy_main_t *sm)
{
    sniproxy_table_t *table;
    pool_get (sm->tables, table);
    clib_memset (table, 0, sizeof (table[0]));
    u64 table_id = table - sm->tables;
      /* Initialise underlying structure */
    table->tree.iprtree_root_node_index = IPRTREE_INVALID_INDEX;
    do_init();
}

void domain_iprtree_insert(sniproxy_main_t *sm, const char *domain, u64 backendsets)
{
    sniproxy_table_t *table;
    sniproxy_pattern_t *pattern;
    u32 table_id = 0;
    clib_error_t *err;
    word i;
    u32 pattern_index;
    if ((table = sniproxy_table_get(sm, table_id)) == NULL)
        fformat(stderr, "table with index: %u not found", 0);

    pool_get_zero (sm->patterns, pattern);
    pattern_index = pattern - sm->patterns;
    pattern->backend_set_index = backendsets;
      /* TODO cover management and incremental insertion deletion */
    pattern->covering_child_index = IPRTREE_INVALID_INDEX;
    pattern->covering_next_index = IPRTREE_INVALID_INDEX;
    pattern->covering_parent_index = IPRTREE_INVALID_INDEX;
    pattern->str = format (0, "%.256s", domain);
    pattern->str = sniproxy_prepare_pattern (pattern->str);

    vec_add1 (table->pattern_indices, pattern_index);
    /*args->table_pattern_id = pattern_index;*/
}
void domain_iprtree_commit(sniproxy_main_t *sm)
{
    sniproxy_table_t *table;
    u32 table_id = 0;

    if ((table = sniproxy_table_get(sm, table_id)) == NULL)
        fformat(stderr, "table with index: %u not found", 0);
    sniproxy_table_rebuild (sm, table);
}

u64 domain_iprtree_search(sniproxy_main_t *sm, const char *domain)
{
    u32 table_id = 0;
    sniproxy_table_t *table = sniproxy_table_get (sm, table_id);
    iprtree_t *tree = &table->tree;
    sniproxy_backend_set_t *bset;
    sniproxy_backend_t *b;
    iprtree_leaf_index_t tgt;

    if (tree->iprtree_root_node_index == IPRTREE_INVALID_INDEX)
      return -1;

    u8 * sni = format(0, "%s%c", domain, 0);
    clib_memmove (sni + 1, sni, vec_len (sni) - 1);
    sni[0] = 0;
    tgt = iprtree_lookup (&sm->iprtree_container, tree, sni, vec_len(sni));

    if (tgt == IPRTREE_INVALID_INDEX)
      return -1;

    vec_free(sni);
    return tgt;
}
