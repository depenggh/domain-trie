/**
 **********************************************************************
 * Copyright (c) 2022 by Cisco Systems, Inc.
 * All rights reserved.
 **********************************************************************
 **/
#ifndef included_sniproxy_h
#define included_sniproxy_h
#include <vlib/vlib.h>
#include "config.h"
#include "iprtree.h"
#include "timer.h"
//#include <vnet/session/application_interface.h>
#include <vppinfra/format_table.h>

#define SNIPROXY_TMP_BUFFER_SZ		(1 << 12)
#define SNIPROXY_INVALID_TABLE_INDEX	((u32) ~0)
#define SNIPROXY_INVALID_INSTANCE_INDEX ((u32) ~0)

#define pool_elt_at_index_safe(P, I)                                          \
  (pool_is_free_index ((P), (I)) ? 0 : pool_elt_at_index ((P), (I)))
#define sniproxy_instance_get(sm, index)                                      \
  pool_elt_at_index_safe ((sm)->instances, (index))
#define sniproxy_listener_get(sm, index)                                      \
  pool_elt_at_index_safe ((sm)->listeners, (index))

#define sniproxy_backendset_get(sm, index)                                    \
  pool_elt_at_index_safe ((sm)->backendsets, (index))

#define sniproxy_pattern_get(sm, index)                                       \
  pool_elt_at_index_safe ((sm)->patterns, (index))

#define sniproxy_table_get(sm, index)                                         \
  pool_elt_at_index_safe ((sm)->tables, (index))

#define sniproxy_session_get(sm, index)                                       \
  pool_elt_at_index_safe ((sm)->po_sessions, (index))

#define sniproxy_listener_count(sm) pool_elts ((sm)->listeners)

#define sniproxy_instance_count(sm) pool_elts ((sm)->instances)

enum
{
  SNIPROXY_TIMER_EVENT_START,
  SNIPROXY_TIMER_EVENT_STOP,
  SNIPROXY_TIMER_EVENT_UPDATE,
  SNIPROXY_TIMER_N_EVENTS
};

typedef struct
{
#define _(name, NAME, str, po_default, ao_default)                            \
  u64 po_##name;                                                              \
  u64 ao_##name;
  foreach_app_option
#undef _
#define _(name, str, defaultval) u64 name;
    foreach_global_option
#undef _
} sniproxy_conf_t;

typedef struct
{
  u32 table_index;
#define _(t, n, x, y) t n;
  foreach_sniproxy_instance_option
#undef _
    u8 is_test_mode;
} sniproxy_instance_t;

typedef struct
{
  u64 handle;
  u32 instance_index;
  u32 fib_index;
} sniproxy_listener_t;

typedef struct
{
  u32 n_instances;
  iprtree_t tree;
  u32 *pattern_indices; /* vec */
} sniproxy_table_t;

typedef struct
{
  //ip46_address_t dst;
  u16 port;	       /* network byte order */
  uword *mapping_list; /* vec of all mappings using this backend */
  u8 is_ip4;
} sniproxy_backend_t;

typedef struct
{
  sniproxy_backend_t *backends; /* pool */
  clib_bitmap_t *active_indices;
  u32 rr_current_bit;
  u8 disable_proxy_header;
  u8 disable_headend_export;
} sniproxy_backend_set_t;

typedef struct
{
  u8 *str;
  u32 covering_parent_index;
  u32 covering_next_index;
  u32 covering_child_index;
  u32 backend_set_index;
} sniproxy_pattern_t;

typedef struct
{
  //ip46_address_t client_ip;
  //ip46_address_t local_ip;
  u16 client_port;
  u16 local_port;
  u8 is_udp;
} sniproxy_session_info_t;
typedef struct
{
  //session_handle_t vpp_server_handle;
  //session_handle_t vpp_active_open_handle;
  volatile int active_open_establishing;
  volatile int po_disconnected;
  volatile int ao_disconnected;
  u32 instance_index;
  u32 po_thread_index;
  u32 mapping_index;
  u32 fib_index;
  u32 timer_handle;
  f64 timeout;
  sniproxy_session_info_t info;
  u8 proxy_protocol_remaining;
  u8 accounted;
  u8 *sni;
  u8 disable_headend_export;
  u32 backendset_id;
} sniproxy_session_t;

typedef struct
{
  clib_spinlock_t lock;
  f64 *next_expirations; /* vec containing expiration time per session_index */
  u8 tmp_proxying_buffer[SNIPROXY_TMP_BUFFER_SZ];
} sniproxy_per_thread_data_t;

typedef struct
{
  //ip46_address_t client_ip;
  uword instance_id;
  u8 sni[];
} sniproxy_2tuple_key_t;

typedef struct
{
  u8 *key; /* vec containing sniproxy_2tuple_key_t */
  //ip46_address_t dst;
  uword ref_cnt;
  u32 backendset_id;
  u16 port; /* network byte order */
  u8 disable_proxy_header;
  u8 disable_headend_export;
  u8 is_ip4;
} sniproxy_2tuple_value_t;

enum
{
#define _(x, s) SNIPROXY_CTR_##x,
  foreach_sniproxy_per_sni_counter
#undef _
    SNIPROXY_N_CTR
};

typedef struct
{
  vlib_simple_counter_main_t cms[SNIPROXY_N_CTR];
  u8 *sni;
} sniproxy_per_sni_stats_t;

typedef struct
{
  sniproxy_per_sni_stats_t *stats_pool; /* pool */
  uword *per_sni_cms;			/* hash */
} sniproxy_stats_t;

typedef struct
{
  sniproxy_conf_t conf;
  sniproxy_stats_t stats;
  clib_spinlock_t sessions_lock;
  sniproxy_per_thread_data_t *ptd;
  sniproxy_instance_t *instances;
  sniproxy_listener_t *listeners;
  sniproxy_session_t *po_sessions;
  sniproxy_backend_set_t *backendsets;
  sniproxy_table_t *tables;
  sniproxy_pattern_t *patterns;
  uword *hash_2tuples;			     /* hash */
  sniproxy_2tuple_value_t *mappings_2tuples; /* pool */
  sniproxy_timer_wheel_t wheel;
  iprtree_container_t iprtree_container;
  u32 active_open_client_index;
  u32 active_open_app_index;
  u32 passive_open_client_index;
  u32 passive_open_app_index;
  u16 msg_id_base;

  /* First worker data */
  clib_spinlock_t connect_lock;
  //vnet_connect_args_t *pending_connects;
} sniproxy_main_t;

typedef struct
{
  u32 listener_id;
  //ip46_address_t addr;
  u32 sw_if_index;
  u16 port;
  //ip_protocol_t proto;
  u32 fib_index;
  u8 is_ip4;
} sniproxy_listener_add_del_args_t;

typedef struct
{
  u32 table_pattern_id;
  u32 table_id;
  u32 backendset_id;
  u8 pattern[256];
} sniproxy_table_pattern_add_del_args_t;

typedef struct
{
  u32 backend_id;
  u32 backendset_id;
  //ip46_address_t ip;
  u16 port; /*host byte order*/
  u8 is_ip4;
} sniproxy_backend_add_del_args_t;

clib_error_t *sniproxy_add_del (sniproxy_main_t *sm, u32 *instance_id,
				u8 is_del);

clib_error_t *sniproxy_set_option (sniproxy_main_t *sm, u32 instance_id,
				   u8 *option_name, u8 *value);

clib_error_t *
sniproxy_listener_add_del (sniproxy_main_t *sm,
			   sniproxy_listener_add_del_args_t *args, u8 is_del);

clib_error_t *sniproxy_listener_attach (sniproxy_main_t *sm, u32 instance_id,
					u32 listener_id);

clib_error_t *sniproxy_listener_detach (sniproxy_main_t *sm, u32 listener_id);

clib_error_t *sniproxy_table_add_del (sniproxy_main_t *sm, u32 *table_id,
				      u8 is_del);

clib_error_t *sniproxy_table_attach (sniproxy_main_t *sm, u32 instance_id,
				     u32 table_id);

clib_error_t *sniproxy_table_detach (sniproxy_main_t *sm, u32 instance_id);

clib_error_t *sniproxy_backendset_add_del (sniproxy_main_t *sm,
					   u32 *backendset_id,
					   u8 disable_proxy_header,
					   u8 disable_headend_export,
					   u8 is_del);

clib_error_t *sniproxy_table_pattern_add_del (
  sniproxy_main_t *sm, sniproxy_table_pattern_add_del_args_t *args, u8 is_del);

clib_error_t *sniproxy_backend_add_del (sniproxy_main_t *sm,
					sniproxy_backend_add_del_args_t *args,
					u8 is_del);

clib_error_t *sniproxy_set_test_mode (sniproxy_main_t *sm, u32 instance_id,
				      u8 is_test);

u8 *sniproxy_prepare_pattern (u8 *pattern);
u8 *sniproxy_unprepare_pattern (u8 *pattern);

void sniproxy_try_close_session_with_index (u32 ps_index, u8 is_active_open,
					    u8 expired);

void sniproxy_session_format_col (table_t *t);

void sniproxy_session_format_cell (table_t *t, u32 n, u32 ps_index, f64 now);

void sniproxy_mapping_format_col (table_t *t);

void sniproxy_mapping_format_cell (table_t *t, u32 n, u32 mapping_index);

extern sniproxy_main_t sniproxy_main;
//extern session_cb_vft_t sniproxy_active_open_vft;
//extern session_cb_vft_t sniproxy_passive_open_vft;

extern vlib_log_class_registration_t sniproxy_log;

#define SNI_LOG_DBG(fmt, ...)                                                 \
  vlib_log_debug (sniproxy_log.class, fmt, __VA_ARGS__)
#define SNI_LOG_ERR(fmt, ...)                                                 \
  vlib_log_err (sniproxy_log.class, fmt, __VA_ARGS__)

#endif /* included_sniproxy_h */
