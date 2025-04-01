
#ifndef DOMAIN_IPRTREE_H
#define DOMAIN_IPRTREE_H

#include <unistd.h>
#include <vppinfra/clib.h>
#include <vppinfra/crc32.h>
#include <vppinfra/format.h>
#include <vppinfra/mem.h>
#include <vppinfra/string.h>
#include <vppinfra/types.h>
#include <vppinfra/pool.h>
#include <vppinfra/vec.h>
#include <vppinfra/bihash_8_8.h>
#include <vppinfra/bihash_template.h>
#include "sniproxy.h"


void domain_iprtree_init(sniproxy_main_t *sm);
void domain_iprtree_insert(sniproxy_main_t *sm, const char *domain, u64 backendsets);
u64 domain_iprtree_search(sniproxy_main_t *sm, const char *domain);
void domain_iprtree_commit(sniproxy_main_t *sm);

#endif
