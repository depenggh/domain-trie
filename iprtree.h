/**
 **********************************************************************
 * Copyright (c) 2022 by Cisco Systems, Inc.
 * All rights reserved.
 **********************************************************************
 **/

#ifndef included_iprtree_h
#define included_iprtree_h
#include <vlib/vlib.h>

#define IPRTREE_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz0123456789-."
#define IPRTREE_ARITY	      (sizeof (IPRTREE_ALLOWED_CHARS))

#define IPRTREE_INVALID_INDEX ((u32) ~0)

typedef enum : u8
{
  IPRTREE_NODE_TYPE_LEAF = 0,
  IPRTREE_NODE_TYPE_INTERNAL = 1
} iptree_node_type_t;

typedef u32 iprtree_node_index_t;
typedef u32 iprtree_leaf_index_t;

typedef struct
{
  iptree_node_type_t type;
  u8 n_children;
  u8 n_skip;
  u8 skip_str[6]; /* Not in reversed order, unconverted chars, only valid for
		     internal nodes */
  u32 ref_cnt;
  union
  {
    iprtree_leaf_index_t target;
    iprtree_node_index_t by_prev_letter[IPRTREE_ARITY];
  };
} iprtree_node_t;

typedef struct
{
  iprtree_node_index_t iprtree_root_node_index;
} iprtree_t;

typedef struct
{
  iprtree_node_t *nodes;
} iprtree_container_t;

extern u8 iprtree_conversion[];
extern u8 inversed_iprtree_conversion[];

static_always_inline iprtree_node_t *
iprtree_node_at_index (iprtree_container_t *container,
		       iprtree_node_index_t index)
{
  return pool_elt_at_index (container->nodes, index);
}

typedef struct
{
  iprtree_node_index_t *current;
  u8 *sibling_index;
} iprtree_iterator_t;

static_always_inline iprtree_iterator_t
iprtree_iterator_begin (iprtree_container_t *container, iprtree_t *tree)
{
  iprtree_iterator_t res = { 0 };
  vec_add1 (res.current, tree->iprtree_root_node_index);
  return res;
}

static_always_inline u8
iprtree_iterator_is_end (iprtree_iterator_t *iterator)
{
  return (vec_len (iterator->current) == 0);
}
static_always_inline void
iprtree_iterator_advance (iprtree_container_t *container,
			  iprtree_iterator_t *iterator)
{
  iprtree_node_t *current_node;
  iprtree_node_index_t current_node_index, next_node_index;
  u8 current_child_index = ~0;

  /* Can't advance the end iterator */
  if (iprtree_iterator_is_end (iterator))
    {
      vec_free (iterator->current);
      vec_free (iterator->sibling_index);
      return;
    }

retry:
  current_node_index = (vec_end (iterator->current) - 1)[0];
  current_node = iprtree_node_at_index (container, current_node_index);

  /* if it's a node with children, keep going */
  if (current_node->type == IPRTREE_NODE_TYPE_INTERNAL &&
      current_node->n_children)
    {
      u8 found = 0;
      for (u8 i = current_child_index + 1; i < IPRTREE_ARITY; i++)
	{
	  if (current_node->by_prev_letter[i] != IPRTREE_INVALID_INDEX)
	    {
	      next_node_index = current_node->by_prev_letter[i];
	      current_child_index = i;
	      found = 1;
	      break;
	    }
	}
      if (found)
	{
	  /* Push node_index on iterator and current_sibling index*/
	  vec_add1 (iterator->current, next_node_index);
	  vec_add1 (iterator->sibling_index, current_child_index);
	  return;
	}
    }
  /* it's either a leaf, or an internal node with no (remaining) children
   * we need to go up in the tree and keep exploring */

  /* Pop the current node index */
  vec_dec_len (iterator->current, 1);

  /* Is it the end? */
  if (vec_len (iterator->current) == 0)
    {
      vec_free (iterator->current);
      vec_free (iterator->sibling_index);
      return;
    }

  /* If not the end when going up in the tree, we MUST have a sibling */
  ASSERT (vec_len (iterator->sibling_index) > 0);

  current_child_index = (vec_end (iterator->sibling_index) - 1)[0];

  /* Pop the current sibling index */
  vec_dec_len (iterator->sibling_index, 1);
  goto retry;
}

static_always_inline iprtree_node_index_t
iprtree_iterator_get_current (iprtree_iterator_t *iterator)
{
  return (vec_end (iterator->current) - 1)[0];
}

#define iprtree_foreach_node(it, container, tree)                             \
  for (it = iprtree_iterator_begin ((container), (tree));                     \
       !iprtree_iterator_is_end (&it);                                        \
       iprtree_iterator_advance (container, &(it)))

static_always_inline iprtree_node_index_t
iprtree_lookup_internal (iprtree_node_t *current_internal_node, u8 *str,
			 uword *remain_len, uword *remain_n_skip,
			 u8 *internal_node_entirely_consumed,
			 u8 *exhausted_str)
{
  u8 converted_char;
  word char_index = *remain_len - 1;
  *remain_n_skip = current_internal_node->n_skip;
  u8 *to_skip = current_internal_node->skip_str + *remain_n_skip - 1;
  str = str + char_index;
  *internal_node_entirely_consumed = 0;
  *exhausted_str = 0;
  while (*remain_len > 0 && *remain_n_skip > 0 && str[0] == to_skip[0])
    {
      *remain_len -= 1;
      *remain_n_skip -= 1;
      str -= 1;
      to_skip -= 1;
    }

  if (*remain_len == 0)
    *exhausted_str = 1;

  if (*remain_n_skip > 0)
    return IPRTREE_INVALID_INDEX;

  *internal_node_entirely_consumed = 1;

  if (*remain_len == 0)
    return IPRTREE_INVALID_INDEX;

  *remain_len -= 1;

  converted_char = iprtree_conversion[str[0]];

  if (converted_char == (u8) ~0)
    return IPRTREE_INVALID_INDEX;

  return current_internal_node->by_prev_letter[converted_char];
}

/**
 * @brief Returns the last valid node index for the inverted longest prefix
 * match (leaf if success, or or node index that failed)
 *
 * @param[in] container The container for iptree nodes
 * @param[in] tree iprtree for the lpm
 * @param[in] str null-terminated character string for the lpm (in original
 * order)
 * @param[in,out] remain_len input: length (including null terminator at the
 * beginning) output: length of unparsed prefix
 * @param[out] n_skip_in_node number of unmatched characters in the skip string
 * of the returned internal node
 * @param[out] last_internal last internal node index that was completely
 * consumed (i.e., skip str & valid child)
 * @param[out] target leaf index if the last valid node is a leaf
 *
 */
static_always_inline iprtree_node_index_t
iprtree_consume_str (iprtree_container_t *container, iprtree_t *tree, u8 *str,
		     uword *remain_len, uword *n_skip_in_node,
		     iprtree_node_index_t *last_internal, u8 *exhausted_str,
		     iprtree_leaf_index_t *target)
{
  iprtree_node_index_t current = tree->iprtree_root_node_index;
  *last_internal = IPRTREE_INVALID_INDEX;
  iprtree_node_t *internal_node;
  iprtree_node_index_t tmp;
  *target = IPRTREE_INVALID_INDEX;
  *last_internal = current;
  u8 internal_node_entirely_consumed;

  /* Handle corner case where tree is empty */
  if (current == IPRTREE_INVALID_INDEX)
    return current;

  do
    {
      internal_node = iprtree_node_at_index (container, current);
      if (internal_node->type == IPRTREE_NODE_TYPE_LEAF)
	{
	  *target = internal_node->target;
	  break;
	}
      tmp = iprtree_lookup_internal (
	internal_node, str, remain_len, n_skip_in_node,
	&internal_node_entirely_consumed, exhausted_str);
      /* if the internal node was entirely consumed, the failed node is the
       * child of internal_node */
      if (tmp == IPRTREE_INVALID_INDEX && internal_node_entirely_consumed)
	{
	  *last_internal = current;
	  current = tmp;
	  break;
	}
      if (tmp == IPRTREE_INVALID_INDEX && !internal_node_entirely_consumed)
	break;
      *last_internal = current;
      current = tmp;
    }
  while (1);

  return current;
};

static_always_inline iprtree_leaf_index_t
iprtree_lookup (iprtree_container_t *container, iprtree_t *tree, u8 *str,
		uword len)
{
  iprtree_leaf_index_t target;
  __clib_unused iprtree_node_index_t result;
  __clib_unused iprtree_node_index_t last_internal;
  __clib_unused u8 exhausted_str;
  uword n_skip_in_node;
      result = iprtree_consume_str (container, tree, str, &len, &n_skip_in_node,
				&last_internal, &exhausted_str, &target);
  return target;
}

static_always_inline iprtree_node_index_t
iprtree_allocate_node (iprtree_container_t *container)
{
  iprtree_node_t *node;
  pool_get (container->nodes, node);
  memset (node, 0, sizeof (node[0]));
  return node - container->nodes;
}

static_always_inline void
iprtree_node_ref_inc (iprtree_node_t *node)
{
  node->ref_cnt += 1;
}

static_always_inline iprtree_node_index_t
iprtree_allocate_internal_node_with_default_leaf (
  iprtree_container_t *container, iprtree_node_index_t default_child)
{
  /* TODO: refactor ifs... */
  iprtree_node_index_t ni = iprtree_allocate_node (container);
  iprtree_node_t *node = iprtree_node_at_index (container, ni);
  iprtree_node_t *leaf;
  node->ref_cnt = 1;

  if (default_child != IPRTREE_INVALID_INDEX)
    leaf = iprtree_node_at_index (container, default_child);

  node->type = IPRTREE_NODE_TYPE_INTERNAL;
  for (int i = 0; i < IPRTREE_ARITY; i++)
    {
      node->by_prev_letter[i] = default_child;
      if (default_child != IPRTREE_INVALID_INDEX)
	{
	  iprtree_node_ref_inc (leaf);
	  node->n_children += 1;
	}
    }

  return ni;
}

static_always_inline iprtree_node_index_t
iprtree_allocate_internal_node (iprtree_container_t *container)
{
  iprtree_node_index_t ni = iprtree_allocate_node (container);
  iprtree_node_t *node = iprtree_node_at_index (container, ni);
  node->ref_cnt = 1;
  node->type = IPRTREE_NODE_TYPE_INTERNAL;
  for (int i = 0; i < IPRTREE_ARITY; i++)
    {
      node->by_prev_letter[i] = IPRTREE_INVALID_INDEX;
    }

  return ni;
}

static_always_inline iprtree_node_index_t
iprtree_allocate_leaf_node (iprtree_container_t *container,
			    iprtree_leaf_index_t li)
{
  iprtree_node_index_t ni = iprtree_allocate_node (container);
  iprtree_node_t *node = iprtree_node_at_index (container, ni);
  node->type = IPRTREE_NODE_TYPE_LEAF;
  node->target = li;
  node->ref_cnt = 1;
  return ni;
}

static_always_inline void
iprtree_free_node (iprtree_container_t *container, iprtree_node_index_t ni)
{
  iprtree_node_t *node = iprtree_node_at_index (container, ni);
  ASSERT (node->ref_cnt > 0);
  node->ref_cnt -= 1;
  if (node->ref_cnt == 0)
    pool_put_index (container->nodes, ni);
}

static_always_inline void
memcpy_reverse (void *dst, void *src, uword len)
{
  u8 *dst2 = dst;
  u8 *src2 = src;
  while (len)
    {
      dst2[len - 1] = src2[0];
      src2 += 1;
      len -= 1;
    }
}
void iprtree_clear (iprtree_container_t *container, iprtree_t *tree);
void iprtree_insert_pattern (iprtree_container_t *container, iprtree_t *tree,
			     u8 *pattern, iprtree_leaf_index_t target);

#endif /* included_iprtree_h */
