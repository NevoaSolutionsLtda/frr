// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _FRR_ROUTING_NB_H_
#define _FRR_ROUTING_NB_H_

#include "lib/hook.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nb_cb_create_args;
struct nb_cb_destroy_args;
struct yang_list_keys;

extern const struct frr_yang_module_info frr_routing_info;
extern const struct frr_yang_module_info frr_routing_cli_info;

/*
 * Iteration providers for oper-state walks over the
 * control-plane-protocol trunk list. The list lives in this library
 * but only protocol daemons know their instances, so a daemon serving
 * operational state under /frr-routing:routing registers these hooks
 * (bgpd is currently the only subscriber). Without a subscriber the
 * callbacks behave as the previous NULL stubs.
 *
 * _oper_next advances (or, with *entry == NULL, starts) the iteration
 * and stores the next opaque entry; _oper_keys fills the list keys
 * (type, name, vrf) of an entry; _oper_lookup resolves list keys to
 * an entry. All return 1 when the request was handled, 0 otherwise.
 */
DECLARE_HOOK(routing_control_plane_protocol_oper_next, (const void **entry),
	     (entry));
DECLARE_HOOK(routing_control_plane_protocol_oper_keys,
	     (const void *entry, struct yang_list_keys *keys), (entry, keys));
DECLARE_HOOK(routing_control_plane_protocol_oper_lookup,
	     (const struct yang_list_keys *keys, const void **entry),
	     (keys, entry));

/* Mandatory callbacks. */
int routing_control_plane_protocols_control_plane_protocol_create(
	struct nb_cb_create_args *args);
int routing_control_plane_protocols_control_plane_protocol_destroy(
	struct nb_cb_destroy_args *args);

#define FRR_ROUTING_XPATH                                                      \
	"/frr-routing:routing/control-plane-protocols/control-plane-protocol"

#define FRR_ROUTING_KEY_XPATH                                                  \
	"/frr-routing:routing/control-plane-protocols/"                        \
	"control-plane-protocol[type='%s'][name='%s'][vrf='%s']"

#define FRR_ROUTING_KEY_XPATH_VRF                                              \
	"/frr-routing:routing/control-plane-protocols/"                        \
	"control-plane-protocol[vrf='%s']"

/*
 * callbacks for routing to handle configuration events
 * based on the control plane protocol
 */
DECLARE_HOOK(routing_conf_event, (struct nb_cb_create_args *args), (args));
DECLARE_HOOK(routing_create, (struct nb_cb_create_args *args), (args));
DECLARE_KOOH(routing_destroy, (struct nb_cb_destroy_args *args), (args));

void routing_control_plane_protocols_register_vrf_dependency(void);

#ifdef __cplusplus
}
#endif

#endif /* _FRR_ROUTING_NB_H_ */
