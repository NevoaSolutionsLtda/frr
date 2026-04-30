/*
 * BGP -lite northbound config callbacks (stubs).
 *
 * Author: MGC Connect / Reinaldo Saraiva <reinaldo.saraiva@magalu.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Every callback in this file is a SCAFFOLD. Each one:
 *   - walks the standard NB transaction FSM (VALIDATE/PREPARE/ABORT/APPLY)
 *   - logs a trace
 *   - returns NB_OK
 *   - carries a TODO(mgc-connect) comment pointing at the design doc
 *     section that specifies the target behavior.
 *
 * See poc/frr-grpc-poc/docs/bgpd_lite_design.md section 3 for the
 * xpath -> FRR CLI mapping that each APPLY branch must realize.
 * See poc/frr-grpc-poc/docs/frr_nb_template.md section 4 for the
 * 4-file pattern these stubs follow.
 */

#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"
#include "log.h"
#include "yang.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgpd_nb_lite.h"

#define BGP_NB_LITE_TRACE(cb, args)                                            \
	zlog_debug("bgpd-lite nb: %s event=%d", (cb), (args)->event)

/* ------------------------------------------------------------------ *
 * 1. /frr-bgpd-lite:bgp/instance  (create / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_create(struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_create", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		as_t as = yang_dnode_get_uint32(args->dnode, "./asn");
		const char *vrf_name =
			yang_dnode_get_string(args->dnode, "./vrf");
		enum bgp_instance_type inst_type =
			(!vrf_name || strmatch(vrf_name, VRF_DEFAULT_NAME))
				? BGP_INSTANCE_TYPE_DEFAULT
				: BGP_INSTANCE_TYPE_VRF;
		struct bgp *bgp = NULL;
		int ret = bgp_get(&bgp, &as, vrf_name, inst_type, NULL,
				  ASNOTATION_UNDEFINED);
		if (ret < 0)
			return NB_ERR;
		nb_running_set_entry(args->dnode, bgp);
		break;
	}
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_destroy(struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_unset_entry(args->dnode);
		if (bgp)
			bgp_delete(bgp);
		break;
	}
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 2. .../instance/router-id  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_router_id_modify(struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_router_id_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		struct in_addr router_id;
		yang_dnode_get_ipv4(&router_id, args->dnode, NULL);
		bgp_router_id_static_set(bgp, router_id);
		break;
	}
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_router_id_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_router_id_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		struct in_addr zero = { .s_addr = INADDR_ANY };
		bgp_router_id_static_set(bgp, zero);
		break;
	}
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 3. .../instance/as-path-multipath-relax  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_as_path_multipath_relax_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_as_path_multipath_relax_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 3 (bgp bestpath as-path multipath-relax).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 4. .../instance/ebgp-requires-policy  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_ebgp_requires_policy_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_ebgp_requires_policy_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 4 ([no] bgp ebgp-requires-policy).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 5. .../instance/network-import-check  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_network_import_check_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_network_import_check_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 5 ([no] bgp network import-check).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 6. .../instance/neighbor  (create / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_create(struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_create", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 6 (neighbor X remote-as Y).
		 * peer_create() + nb_running_set_entry().
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_destroy(struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 6 (no neighbor X).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 7. .../neighbor/remote-as  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_remote_as_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_remote_as_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 7 (neighbor X remote-as Y, change).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 8. .../neighbor/bfd  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_bfd_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_bfd_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 8 ([no] neighbor X bfd).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_bfd_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_bfd_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 8 (no neighbor X bfd).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 9. .../neighbor/description  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_description_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_description_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 9 (neighbor X description STR).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_description_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_description_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 9 (no neighbor X description).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 10. .../neighbor/timers  (per-leaf modify + atomic apply_finish)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_timers_keepalive_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_timers_keepalive_modify",
			  args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 10 (neighbor X timers KA HT).
		 * Stage new keepalive; final peer_timers_set() fires in
		 * apply_finish so both timers commit atomically.
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_timers_hold_time_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_timers_hold_time_modify",
			  args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 10. Stage new hold-time for apply_finish.
		 */
		break;
	}
	return NB_OK;
}

void frr_bgpd_lite_bgp_instance_neighbor_timers_apply_finish(
	struct nb_cb_apply_finish_args *args)
{
	/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
	 * section 3 row 10 (atomic neighbor X timers KA HT).
	 * Read ./keepalive + ./hold-time via yang_dnode_get_uint16()
	 * and call peer_timers_set(peer, ka, ht).
	 */
	zlog_debug("bgpd-lite nb: neighbor_timers_apply_finish");
}

/* ------------------------------------------------------------------ *
 * 11. .../neighbor/address-family  (create / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_create(
	struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_address_family_create", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 11 (enter address-family ipv4 unicast).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_address_family_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_address_family_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 11 (exit-address-family + deactivate).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 12. .../neighbor/address-family/activate  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_activate_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_activate_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 12 (neighbor X activate).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 13. .../neighbor/address-family/soft-reconfiguration-inbound  (modify)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_soft_reconfiguration_inbound_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_soft_reconfiguration_inbound_modify",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 13 (neighbor X soft-reconfiguration inbound).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 14. .../neighbor/address-family/route-map/import  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_route_map_import_modify",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 14 (neighbor X route-map NAME in).
		 * Call peer_route_map_set(peer, AFI_IP, SAFI_UNICAST,
		 * RMAP_IN, name).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_route_map_import_destroy",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 14 (no neighbor X route-map NAME in).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 15. .../neighbor/address-family/route-map/export  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_route_map_export_modify",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 15 (neighbor X route-map NAME out).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_route_map_export_destroy",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 15 (no neighbor X route-map NAME out).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 16. .../neighbor/address-family/maximum-prefix  (modify / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_maximum_prefix_modify",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 16 (neighbor X maximum-prefix N).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_neighbor_address_family_maximum_prefix_destroy",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 16 (no neighbor X maximum-prefix).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 17. .../address-family  (global list stubs -- required by NB validator)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_address_family_create(
	struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_create", args);
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_address_family_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_destroy", args);
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 17a. .../address-family/network  (create / destroy)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_address_family_network_create(
	struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_network_create", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 17 (network A.B.C.D/M).
		 * bgp_static_set() on parent struct bgp.
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_address_family_network_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_network_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 17 (no network A.B.C.D/M).
		 */
		break;
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 18. .../address-family/redistribute  (create / destroy + route-map)
 * ------------------------------------------------------------------ */

int frr_bgpd_lite_bgp_instance_address_family_redistribute_create(
	struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_redistribute_create",
			  args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 18 (redistribute PROTO).
		 * bgp_redistribute_set(bgp, AFI_IP, SAFI_UNICAST, proto).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_address_family_redistribute_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_address_family_redistribute_destroy",
			  args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 18 (no redistribute PROTO).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_address_family_redistribute_route_map_modify",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 18 (redistribute PROTO route-map NAME).
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_address_family_redistribute_route_map_destroy",
		args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY:
		/* TODO(mgc-connect): implement -- see bgpd_lite_design.md
		 * section 3 row 18 (no redistribute PROTO route-map).
		 */
		break;
	}
	return NB_OK;
}
