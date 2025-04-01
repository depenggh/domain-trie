#ifndef included_sniproxy_timer_h
#define included_sniproxy_timer_h
#include <vlib/vlib.h>
void sniproxy_expired_timers (u32 *expired);
extern vlib_node_registration_t sniproxy_timer_process_node;
#include <vppinfra/tw_timer_1t_3w_1024sl_ov.h>

typedef TWT (tw_timer_wheel) sniproxy_timer_wheel_t;
typedef TWT (tw_timer) sniproxy_timer_t;

/* Forward all timer functions */
#define foreach_tw_function                                                   \
  _ (start)                                                                   \
  _ (stop)                                                                    \
  _ (handle_is_free)                                                          \
  _ (update)                                                                  \
  _ (wheel_init)                                                              \
  _ (wheel_free)                                                              \
  _ (expire_timers)                                                           \
  _ (expire_timers_vec)

#define _(x) extern typeof (TW (tw_timer_##x)) *sniproxy_timer_##x;
foreach_tw_function
#if TW_FAST_WHEEL_BITMAP
_ (first_expires_in_ticks)
#endif
#undef _
#endif /* included_sniproxy_timer_h */