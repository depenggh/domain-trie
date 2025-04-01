#ifndef included_sniproxy_config_h
#define included_sniproxy_config_h

/* name, NAME, string, PO default, AO default */
#define foreach_app_option                                                    \
  _ (segment_size, SEGMENT_SIZE, "segment-size", 1 << 30, 1 << 30)            \
  _ (add_segment_size, ADD_SEGMENT_SIZE, "add-segment-size", 1 << 24,         \
     1 << 24)                                                                 \
  _ (rx_fifo_size, RX_FIFO_SIZE, "rx-fifo-size", 4 << 20, 4 << 20)            \
  _ (tx_fifo_size, TX_FIFO_SIZE, "tx-fifo-size", 4 << 20, 4 << 20)            \
  _ (max_fifo_size, MAX_FIFO_SIZE, "max-fifo-size", 128 << 20, 128 << 20)     \
  _ (high_watermark, HIGH_WATERMARK, "high-watermark", 80, 80)                \
  _ (low_watermark, LOW_WATERMARK, "low-watermark", 50, 50)                   \
  _ (private_segment_count, PRIVATE_SEGMENT_COUNT, "private-segment-count",   \
     0, 0)                                                                    \
  _ (prealloc_fifo_pairs, PREALLOC_FIFO_PAIRS, "prealloc-fifo-pairs", 0, 0)

#define foreach_global_option _ (max_fwd_size, "max-fwd-size", 64 << 10)

#define SNIPROXY_FORMAT_STR_f64 "%f"
#define foreach_sniproxy_instance_option                                      \
  _ (f64, udp_timeout, "udp-timeout", 120)                                    \
  _ (f64, tcp_timeout, "tcp-timeout", 120)

#define foreach_sniproxy_per_sni_counter                                      \
  _ (UDP_CREATED_SESSIONS, "udp-created")                                     \
  _ (UDP_DELETED_SESSIONS, "udp-deleted")                                     \
  _ (TCP_CREATED_SESSIONS, "tcp-created")                                     \
  _ (TCP_DELETED_SESSIONS, "tcp-deleted")

#define SNIPROXY_TW_INTERVAL_SECONDS 1.0f
#define SNIPROXY_TEST_SNI	     "test.sniproxy"
#define SNIPROXY_QUIC_BACKENDSET     "quic.backendset"
#endif /* included_sniproxy_config_h */