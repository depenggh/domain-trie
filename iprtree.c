#include "iprtree.h"
#include "sniproxy.h"
#include "vlib/main.h"

u8 iprtree_conversion[256];
u8 inversed_iprtree_conversion[256];
void
iprtree_clear (iprtree_container_t *container, iprtree_t *tree)
{
  iprtree_iterator_t it;
  iprtree_node_index_t *to_remove = 0, *current = 0;

  if (tree->iprtree_root_node_index == IPRTREE_INVALID_INDEX)
    return;

  iprtree_foreach_node (it, container, tree)
    vec_add1 (to_remove, iprtree_iterator_get_current (&it));

  vec_foreach (current, to_remove)
    {
      iprtree_free_node (container, current[0]);
      /* pool_put_index (container->nodes, current[0]); */
    }

  tree->iprtree_root_node_index = IPRTREE_INVALID_INDEX;

  vec_free (to_remove);
}
static_always_inline void
iprtree_internal_node_set_child (iprtree_container_t *container,
				 iprtree_node_t *node, u8 char_to_set,
				 iprtree_node_index_t target)
{
  u8 converted;
  iprtree_node_index_t old_target;
  iprtree_node_t /**old_target_node, */ *target_node = NULL;

  converted = iprtree_conversion[char_to_set];
  old_target = node->by_prev_letter[converted];
  if (old_target != IPRTREE_INVALID_INDEX)
    {
      /*old_target_node = iprtree_node_at_index (container, old_target);*/
      node->n_children -= 1;
    }
  if (target != IPRTREE_INVALID_INDEX)
    {
      node->n_children += 1;
      target_node = iprtree_node_at_index (container, target);
    }
  node->by_prev_letter[converted] = target;
  if (target != old_target)
    {
      if (target_node)
	iprtree_node_ref_inc (target_node);

      if (old_target != IPRTREE_INVALID_INDEX)
	iprtree_free_node (container, old_target);
    }
};

void
iprtree_internal_node_set_default_target (
  iprtree_container_t *container, iprtree_node_index_t internal_node_index,
  iprtree_node_index_t target_node)
{
  iprtree_t subtree;
  iprtree_iterator_t it;
  subtree.iprtree_root_node_index = internal_node_index;
  iprtree_node_index_t *to_process = 0;
  iprtree_node_index_t *to_process_current = 0;
  iprtree_foreach_node (it, container, &subtree)
  {
    iprtree_node_index_t ni = iprtree_iterator_get_current (&it);
    iprtree_node_t *current_node = iprtree_node_at_index (container, ni);
    if (current_node->type == IPRTREE_NODE_TYPE_INTERNAL)
      vec_add1 (to_process, ni);
  }
  vec_foreach (to_process_current, to_process)
    {
      iprtree_node_index_t lni = to_process_current[0];
      iprtree_node_t *base_node =
	iprtree_node_at_index (container, to_process_current[0]);
      iprtree_node_t *current_node;
      iprtree_node_index_t saved_children[IPRTREE_ARITY];
      clib_memcpy (&saved_children, &base_node->by_prev_letter,
		   sizeof (base_node->by_prev_letter));
      /* Clear children of the base node */
      for (int i = 0; i < IPRTREE_ARITY; i++)
	{
	  if (base_node->by_prev_letter[i] != IPRTREE_INVALID_INDEX)
	    {
	      /* lock it */
	      current_node = iprtree_node_at_index (
		container, base_node->by_prev_letter[i]);
	      iprtree_node_ref_inc (current_node);
	    }
	  iprtree_internal_node_set_child (
	    container, base_node, inversed_iprtree_conversion[i], target_node);
	  base_node = iprtree_node_at_index (container, to_process_current[0]);
	}

      current_node = iprtree_node_at_index (container, lni);
      while (base_node->n_skip)
	{
	  u8 last_char = base_node->skip_str[base_node->n_skip - 1];
	  base_node->n_skip -= 1;
	  iprtree_node_index_t nni =
	    iprtree_allocate_internal_node_with_default_leaf (container,
							      target_node);
	  current_node = iprtree_node_at_index (container, lni);
	  iprtree_internal_node_set_child (container, current_node, last_char,
					   nni);
	  iprtree_free_node (container, nni);

	  base_node = iprtree_node_at_index (container, to_process_current[0]);
	  lni = nni;
	}
      current_node = iprtree_node_at_index (container, lni);

      /* Restore saved children */
      for (int i = 0; i < IPRTREE_ARITY; i++)
	{
	  if (saved_children[i] != IPRTREE_INVALID_INDEX)
	    {
	      iprtree_internal_node_set_child (container, current_node,
					       inversed_iprtree_conversion[i],
					       saved_children[i]);
	      iprtree_free_node (container, saved_children[i]);
	    }
	}
    }

  vec_free (to_process);
}

void
iprtree_insert_pattern (iprtree_container_t *container, iprtree_t *tree,
			u8 *pattern, iprtree_leaf_index_t target)
{
  iprtree_node_index_t ni, nni, nli;
  iprtree_node_index_t ini;
  iprtree_node_t *node, *new_node;
  iprtree_node_t *internal_node;
  iprtree_leaf_index_t old_target;
  uword remain_len;
  uword n_skip_in_node;
  u8 last_char;
  u8 exhausted_str;

  remain_len = vec_len (pattern);
  ni =
    iprtree_consume_str (container, tree, pattern, &remain_len,
			 &n_skip_in_node, &ini, &exhausted_str, &old_target);

  if (old_target != IPRTREE_INVALID_INDEX)
    {
      node = iprtree_node_at_index (container, ni);
      internal_node = iprtree_node_at_index (container, ini);

      /* We matched a leaf! */
      ASSERT (node->type == IPRTREE_NODE_TYPE_LEAF);
      last_char = pattern[remain_len];

      /* Insert intermediate internal nodes if not exact match */
      iprtree_node_ref_inc (node);
      while (remain_len > 0)
	{
	  nni =
	    iprtree_allocate_internal_node_with_default_leaf (container, ni);
	  new_node = iprtree_node_at_index (container, nni);
	  internal_node = iprtree_node_at_index (container, ini);

	  /* Only skip strings if we are not matching an existing wildcard
	   * pattern */
	  if (ni == IPRTREE_INVALID_INDEX)
	    new_node->n_skip =
	      clib_min (remain_len - 1, ARRAY_LEN (new_node->skip_str));

	  clib_memcpy (new_node->skip_str,
		       pattern + remain_len - new_node->n_skip,
		       new_node->n_skip);
	  remain_len -= new_node->n_skip;
	  iprtree_internal_node_set_child (container, internal_node, last_char,
					   nni);
	  /* Don't need a ref to the inserted node anymore because it's in the
	   * tree */
	  iprtree_free_node (container, nni);
	  ASSERT (remain_len > 0);
	  remain_len -= 1;
	  last_char = pattern[remain_len];
	  ini = nni;
	}
      node = iprtree_node_at_index (container, ni);
      internal_node = iprtree_node_at_index (container, ini);

      /* If unique ref to the leaf, can be reused */
      if (node->ref_cnt == 1)
	{
	  node->target = target;
	  iprtree_internal_node_set_child (container, internal_node, last_char,
					   ni);
	}
      else
	{
	  /* Need to allocate a new leaf */
	  nni = iprtree_allocate_leaf_node (container, target);
	  internal_node = iprtree_node_at_index (container, ini);

	  iprtree_internal_node_set_child (container, internal_node, last_char,
					   nni);
	  iprtree_free_node (container, nni);
	}
      iprtree_free_node (container, ni);
    }
  else
    {
      /* We reached an internal node, not a leaf */
      /* Failed the skip_str ?*/
      if (ni != IPRTREE_INVALID_INDEX && n_skip_in_node > 0)
	{
	  u8 total_to_be_skipped;
	  /* Create intermediary node exactly where the failure happened */
	  nni = iprtree_allocate_internal_node (container);

	  new_node = iprtree_node_at_index (container, nni);
	  node = iprtree_node_at_index (container, ni);
	  internal_node = iprtree_node_at_index (container, ini);
	  total_to_be_skipped = node->n_skip;
	  /* The failed char */
	  last_char = node->skip_str[n_skip_in_node - 1];

	  /* Copy what was already matched in the new node */
	  new_node->n_skip = node->n_skip - n_skip_in_node;
	  clib_memcpy (new_node->skip_str,
		       &node->skip_str[node->n_skip - new_node->n_skip],
		       new_node->n_skip);
	  /* Trim what was already matched off the old node + 1 */
	  node->n_skip = n_skip_in_node - 1;

	  /* Insert the old internal node as kid of the new one */
	  iprtree_internal_node_set_child (container, new_node, last_char, ni);

	  /* Insert the new internal node as kid of the last successful
	   * internal node */
	  internal_node = iprtree_node_at_index (container, ini);
	  iprtree_internal_node_set_child (
	    container, internal_node,
	    pattern[remain_len + total_to_be_skipped - n_skip_in_node], nni);

	  /* The new internal node is in tree, we don't need a ref anymore */
	  iprtree_free_node (container, nni);

	  ini = nni;
	  ni = IPRTREE_INVALID_INDEX;
	  n_skip_in_node = 0;

	  /* Set ourselves up in the invalid index case */
	  remain_len -= 1;
	}
      internal_node = iprtree_node_at_index (container, ini);
      /* Current issue is that we have not reached a leaf because of invalid
       * index or because of exhausted string  */
      /* If string is exhausted, it means that we are matching a wildcard
       * because 0 can only be consumed by a leaf */
      nli = iprtree_allocate_leaf_node (container, target);
      if (exhausted_str)
	{
	  internal_node = iprtree_node_at_index (container, ini);
	  /* No other reason to stop except string exhaustion */
	  // ASSERT (remain_len == 0);

	  /* String exhaustion means that we haven't stop on 0 because 0 is
	   * necessary a leaf* so we are inserting a wildcard pattern */
	  ASSERT (pattern[0] != 0);

	  for (int i = 0; i < IPRTREE_ARITY; i++)
	    {
	      if (internal_node->by_prev_letter[i] == IPRTREE_INVALID_INDEX)
		{
		  iprtree_internal_node_set_child (
		    container, internal_node, inversed_iprtree_conversion[i],
		    nli);
		}
	      else
		{
		  /* Set default value for the internal node */
		  iprtree_internal_node_set_default_target (
		    container, internal_node->by_prev_letter[i], nli);
		}
	      internal_node = iprtree_node_at_index (container, ini);
	    }
	}
      else
	{
	  last_char = pattern[remain_len];
	  ASSERT (ni == IPRTREE_INVALID_INDEX);
	  internal_node = iprtree_node_at_index (container, ini);
	  while (remain_len > 0)
	    {
	      nni = iprtree_allocate_internal_node_with_default_leaf (
		container, IPRTREE_INVALID_INDEX);
	      new_node = iprtree_node_at_index (container, nni);
	      internal_node = iprtree_node_at_index (container, ini);

	      new_node->n_skip =
		clib_min (remain_len - 1, ARRAY_LEN (new_node->skip_str));
	      clib_memcpy (new_node->skip_str,
			   pattern + remain_len - new_node->n_skip,
			   new_node->n_skip);
	      remain_len -= new_node->n_skip;
	      ASSERT (remain_len > 0);
	      iprtree_internal_node_set_child (container, internal_node,
					       last_char, nni);
	      /* Don't need a ref to the new node */
	      iprtree_free_node (container, nni);
	      remain_len -= 1;
	      last_char = pattern[remain_len];
	      ini = nni;
	      internal_node = new_node;
	    }
	  internal_node = iprtree_node_at_index (container, ini);
	  iprtree_internal_node_set_child (container, internal_node, last_char,
					   nli);
	}
      iprtree_free_node (container, nli);
    }
}

static clib_error_t *
iprtree_init (vlib_main_t *vm)
{
  const char *allowed = IPRTREE_ALLOWED_CHARS;
  u8 val = 0;

  for (int i = 0; i < ARRAY_LEN (iprtree_conversion); i++)
    iprtree_conversion[i] = ~(u8) 0;

  for (int i = 0; i < ARRAY_LEN (inversed_iprtree_conversion); i++)
    inversed_iprtree_conversion[i] = ~(u8) 0;

  /* Handle the termination character (epsilon) */
  iprtree_conversion[0] = val++;
  inversed_iprtree_conversion[0] = 0;

  while (allowed[0])
    {
      iprtree_conversion[(u8) allowed[0]] = val;
      inversed_iprtree_conversion[val] = (u8) allowed[0];
      allowed += 1;
      val += 1;
    }
  return 0;
}


void do_init() {
    iprtree_init ((vlib_main_t *)NULL);
}
//VLIB_INIT_FUNCTION (iprtree_init);
