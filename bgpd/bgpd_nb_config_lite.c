/*
 * BGP -lite northbound config callbacks.
 *
 * Author: MGC Connect / Reinaldo Saraiva <reinaldo.saraiva@magalu.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Each callback walks the standard NB transaction FSM
 * (VALIDATE/PREPARE/ABORT/APPLY), logs a trace, and either:
 *   - mutates struct bgp/peer in NB_EV_APPLY (wired callback), or
 *   - carries a TODO(mgc-connect) comment in NB_EV_APPLY pointing at
 *     the design doc row that specifies the target behavior (stub).
 *
 * Reference wiring pattern (first wired in row 4 / ebgp-requires-policy
 * during 4i-BR, ADR-025 first deliverable):
 *
 *   case NB_EV_APPLY: {
 *       struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
 *       <type> val = yang_dnode_get_<type>(args->dnode, NULL);
 *       SET_FLAG/UNSET_FLAG/<runtime call>(bgp, val);
 *       break;
 *   }
 *
 * The runtime call must mirror the legacy CLI handler in
 * bgp_vty.c/bgp_route.c so that vtysh "show running-config" renders the
 * same lines whether the configuration arrived via lite-YANG or vty.
 *
 * See poc/frr-grpc-poc/docs/bgpd_lite_design.md section 3 for the
 * xpath -> FRR CLI mapping that each APPLY branch must realize.
 * See poc/frr-grpc-poc/docs/frr_nb_template.md section 4 for the
 * 4-file pattern these callbacks follow.
 */

#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"
#include "log.h"
#include "sockunion.h"
#include "yang.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_bfd.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_zebra.h"   /* bgp_redistribute_set / bgp_redistribute_unset */
#include "bgpd/bgp_evpn.h"    /* bgp_evpn_cleanup_on_disable (4i-CH) */
#include "bgpd/bgpd_nb_lite.h"
#include "lib/routemap.h"     /* route_map_lookup_by_name, MTYPE_ROUTE_MAP_NAME (4i-BW) */

#define BGP_NB_LITE_TRACE(cb, args)                                            \
	zlog_debug("bgpd-lite nb: %s event=%d", (cb), (args)->event)

/* ------------------------------------------------------------------ *
 * Helper: resolve afi/safi from any dnode within an `address-family`
 * list-entry subtree. Walks ancestors until it finds the AF list-entry
 * (yang_dnode_get_parent matches schema name == "address-family"),
 * then maps the canonical enum string from the `afi-safi` key to FRR
 * AFI_X / SAFI_X constants. Works at any depth (activate leaf is direct
 * child; route-map/import|export leaves are nested under route-map).
 *
 * Added in 4i-CH (ADR-026) for l2vpn-evpn AF support; previous
 * callbacks hard-coded AFI_IP/SAFI_UNICAST.
 *
 * Returns 0 on success (afi/safi populated), -1 otherwise.
 * ------------------------------------------------------------------ */
static int lite_afi_safi_from_dnode(const struct lyd_node *dnode,
				     afi_t *afi, safi_t *safi)
{
	const struct lyd_node *af = yang_dnode_get_parent(dnode,
							  "address-family");
	if (!af)
		return -1;

	const char *name = yang_dnode_get_string(af, "afi-safi");
	if (strcmp(name, "ipv4-unicast") == 0) {
		*afi  = AFI_IP;
		*safi = SAFI_UNICAST;
		return 0;
	}
	if (strcmp(name, "l2vpn-evpn") == 0) {
		*afi  = AFI_L2VPN;
		*safi = SAFI_EVPN;
		return 0;
	}
	return -1;
}

/* W1 (4i-CI): VALIDATE-phase guard for AF-aware callbacks. Rejects
 * unknown afi-safi enum values during NB_EV_VALIDATE so mgmtd refuses
 * the candidate-datastore commit before APPLY runs, matching the
 * northbound contract for early rejection of invalid configurations.
 * APPLY-phase guards remain in place as defensive fallbacks because
 * mgmtd stale-state replay paths can deliver an APPLY without a
 * preceding VALIDATE (per northbound.c worker semantics).
 *
 * Caller passes args->errmsg / args->errmsg_len from either
 * nb_cb_modify_args or nb_cb_destroy_args -- both share that pair.
 *
 * Returns NB_OK on known enum, NB_ERR_VALIDATION on unknown.
 */
static int lite_afi_safi_validate(const struct lyd_node *dnode,
				   char *errmsg, size_t errmsg_len)
{
	afi_t afi;
	safi_t safi;

	if (lite_afi_safi_from_dnode(dnode, &afi, &safi) != 0) {
		snprintf(errmsg, errmsg_len,
			 "bgpd-lite: unknown afi-safi enum");
		return NB_ERR_VALIDATION;
	}
	return NB_OK;
}

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
		bool is_default = (!vrf_name ||
				   strmatch(vrf_name, VRF_DEFAULT_NAME));
		enum bgp_instance_type inst_type =
			is_default ? BGP_INSTANCE_TYPE_DEFAULT
				   : BGP_INSTANCE_TYPE_VRF;
		/* W5 fix (4i-CK.2): bgp_lookup_by_name_filter matches
		 * bgp->name strictly, but the default instance from
		 * frr.conf has name=NULL. Passing the literal
		 * "default" string would miss the lookup and create a
		 * second default instance, leaving every per-neighbor
		 * mutation orphan. Mirror the vanilla CLI parser, which
		 * collapses both "router bgp X" and "router bgp X vrf
		 * default" onto name=NULL. */
		const char *bgp_name = is_default ? NULL : vrf_name;
		struct bgp *bgp = NULL;
		int ret = bgp_get(&bgp, &as, bgp_name, inst_type, NULL,
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		bool enabled = yang_dnode_get_bool(args->dnode, NULL);

		if (enabled)
			SET_FLAG(bgp->flags, BGP_FLAG_ASPATH_MULTIPATH_RELAX);
		else
			UNSET_FLAG(bgp->flags, BGP_FLAG_ASPATH_MULTIPATH_RELAX);

		/* Vanilla CLI accepts an [as-set|no-as-set] sub-arg that toggles
		 * BGP_FLAG_MULTIPATH_RELAX_AS_SET; -lite YANG omits that knob
		 * (design doc §7 row 7 — no-as-set is the documented default).
		 * Defensively clear the flag so any residue from a prior legacy
		 * `bgp bestpath as-path multipath-relax as-set` does not leak
		 * into our managed state.
		 */
		UNSET_FLAG(bgp->flags, BGP_FLAG_MULTIPATH_RELAX_AS_SET);

		bgp_recalculate_all_bestpaths(bgp);
		break;
	}
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		bool enabled = yang_dnode_get_bool(args->dnode, NULL);

		if (enabled)
			SET_FLAG(bgp->flags, BGP_FLAG_EBGP_REQUIRES_POLICY);
		else
			UNSET_FLAG(bgp->flags, BGP_FLAG_EBGP_REQUIRES_POLICY);
		break;
	}
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		bool enabled = yang_dnode_get_bool(args->dnode, NULL);
		bool was_enabled = CHECK_FLAG(bgp->flags, BGP_FLAG_IMPORT_CHECK);

		/* Vanilla bgp_vty.c only re-runs the static-route import check
		 * on a real state change; mirror that to avoid an unnecessary
		 * RIB walk when the value re-asserts the current setting.
		 */
		if (enabled && !was_enabled) {
			SET_FLAG(bgp->flags, BGP_FLAG_IMPORT_CHECK);
			bgp_static_redo_import_check(bgp);
		} else if (!enabled && was_enabled) {
			UNSET_FLAG(bgp->flags, BGP_FLAG_IMPORT_CHECK);
			bgp_static_redo_import_check(bgp);
		}
		break;
	}
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *remote_addr_str =
			yang_dnode_get_string(args->dnode, "./remote-address");
		as_t remote_as =
			yang_dnode_get_uint32(args->dnode, "./remote-as");
		union sockunion su;
		struct peer *peer;
		char as_str[12];
		int ret;

		if (str2sockunion(remote_addr_str, &su) < 0)
			return NB_ERR;

		snprintf(as_str, sizeof(as_str), "%u", remote_as);
		ret = peer_remote_as(bgp, &su, NULL, &remote_as,
				     AS_SPECIFIED, as_str);
		if (ret < 0)
			return NB_ERR;

		peer = peer_lookup(bgp, &su);
		if (!peer)
			/* peer_remote_as just created it; only hash
			 * corruption could make this lookup fail. */
			return NB_ERR;
		nb_running_set_entry(args->dnode, peer);
		break;
	}
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
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_unset_entry(args->dnode);

		if (peer)
			peer_delete(peer);
		break;
	}
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
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		as_t new_as = yang_dnode_get_uint32(args->dnode, NULL);
		char as_str[12];

		/* Mirror vanilla peer_remote_as() guard (bgpd.c:2394-2397):
		 * skip the per-call session reset when the AS has not actually
		 * changed.  This matters when neighbor_create has just set the
		 * AS via peer_remote_as() and mgmtd subsequently emits a
		 * same-value modify in the same APPLY pass.
		 */
		if (peer->as != new_as || peer->as_type != AS_SPECIFIED) {
			snprintf(as_str, sizeof(as_str), "%u", new_as);
			peer_as_change(peer, new_as, AS_SPECIFIED, as_str);
		}
		break;
	}
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
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		bool bfd = yang_dnode_get_bool(args->dnode, NULL);

		/* Mirror vanilla `neighbor X bfd` / `no neighbor X bfd`:
		 * bgp_bfd.c:516-537 (HAVE_BFDD path).
		 * bgp_peer_config_apply propagates session changes; safe when
		 * peer->group == NULL (bgp_bfd.c:128-156 handles NULL group).
		 */
		if (bfd) {
			bgp_peer_configure_bfd(peer, true);
			bgp_peer_config_apply(peer, peer->group);
		} else {
			bgp_peer_remove_bfd_config(peer);
		}
		break;
	}
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
		/* Not reached: the registry for `./neighbor/bfd` registers only
		 * `.modify` (the leaf carries `default "false"` in the YANG, so
		 * "removing" it is modelled as modify(false), not destroy).
		 * Disabling BFD is handled in bfd_modify above.
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
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		const char *desc = yang_dnode_get_string(args->dnode, NULL);

		/* Mirror vanilla DEFUN `neighbor X description STR`
		 * (bgp_vty.c:7882..7906): peer_description_set XSTRDUPs
		 * the string internally so passing the YANG-owned dnode
		 * value is safe.  Returns void; YANG `length 1..80`
		 * already validated in the candidate.
		 */
		peer_description_set(peer, desc);
		break;
	}
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
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);

		/* Mirror vanilla DEFUN `no neighbor X description`
		 * (bgp_vty.c:7908..7926): peer_description_unset is
		 * idempotent (XFREE is NULL-safe).
		 */
		peer_description_unset(peer);
		break;
	}
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
	case NB_EV_APPLY:
		/* NO-OP by design: peer_timers_set takes both timers
		 * atomically.  apply_finish is registered on the parent
		 * `timers` container (bgpd_nb_lite.c:98) and dispatches
		 * exactly once per commit even when both leaves change
		 * (lib/northbound.c:2235 dedupes).  Calling
		 * peer_timers_set here per-leaf would write peer->holdtime
		 * and peer->keepalive twice with an intermediate state
		 * where one leaf carries its OLD candidate value while
		 * peer_timers_set normalizes keepalive against the wrong
		 * holdtime (bgpd.c:6776 clamps ka to ht/3).  apply_finish
		 * guarantees a single write with the final consistent pair.
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
	case NB_EV_APPLY:
		/* NO-OP by design — see keepalive_modify above. */
		break;
	}
	return NB_OK;
}

void frr_bgpd_lite_bgp_instance_neighbor_timers_apply_finish(
	struct nb_cb_apply_finish_args *args)
{
	struct peer *peer;
	uint16_t ka, ht;
	int ret;

	/* args->dnode is the `timers` container; nb_running_get_entry
	 * ascends to the peer registered by neighbor_create.  Children
	 * are read via the canonical `./<leaf>` form (mirrors lite stub
	 * conventions and matches frr-bgpd-lite.yang grouping bgp-timers).
	 *
	 * peer_timers_set (bgpd.c:6756) atomically updates
	 * peer->keepalive + peer->holdtime + PEER_FLAG_TIMER and is
	 * idempotent on re-application.  YANG `must . >= (../keepalive
	 * * 3)` rejects invalid pairs at commit, so the int return is
	 * defensive only — log a warning on the unexpected non-zero so
	 * future YANG drift surfaces in the log instead of silently.
	 */
	peer = nb_running_get_entry(args->dnode, NULL, true);
	ka = yang_dnode_get_uint16(args->dnode, "./keepalive");
	ht = yang_dnode_get_uint16(args->dnode, "./hold-time");

	zlog_debug("bgpd-lite nb: neighbor_timers_apply_finish ka=%u ht=%u",
		   ka, ht);

	ret = peer_timers_set(peer, (uint32_t)ka, (uint32_t)ht);
	if (ret != 0)
		zlog_warn("bgpd-lite nb: peer_timers_set returned %d (ka=%u ht=%u); YANG `must` likely drifted",
			  ret, ka, ht);
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
		/* Intentional skeletal NO-OP (ADR-025, design doc §3 row 11).
		 * Entering address-family scope has no struct peer mutation in
		 * vanilla FRR (`address-family ipv4 unicast` only changes the
		 * VTY node). The actual activation is driven by activate_modify
		 * (row 12). nb_running_set_entry is deliberately not called
		 * here so that activate_modify's ancestor walk skips this
		 * list-entry and resolves the peer registered in neighbor_create.
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_neighbor_address_family_destroy(
	struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_neighbor_address_family_destroy",
			  args);

	switch (args->event) {
	case NB_EV_VALIDATE:
		return lite_afi_safi_validate(args->dnode, args->errmsg,
					      args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		/* Mirror of activate_modify(false) for the Remove-AF path.
		 * Without this, mgmtd tree clears but bgpd vanilla state
		 * retains `neighbor X activate` under the AF (W4, ADR-026
		 * 4i-CJ). Removing the parent neighbor still goes through
		 * peer_delete and is handled separately.
		 */
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		afi_t afi;
		safi_t safi;
		int ret;

		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: AF destroy unknown afi-safi");
			break;
		}

		ret = peer_deactivate(peer, afi, safi);
		if (ret != 0)
			zlog_warn("bgpd-lite: peer_deactivate ret=%d on AF destroy",
				  ret);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		/* args->dnode is the `activate` leaf, 2 levels below the
		 * `neighbor` list-entry.  address_family_create is a skeletal
		 * NO-OP and does not call nb_running_set_entry, so the ancestor
		 * walk skips the AF list-entry and resolves the peer set by
		 * neighbor_create (confirmed: northbound.c nb_running_get_entry_
		 * worker loops until an entry is found).
		 */
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		bool activate = yang_dnode_get_bool(args->dnode, NULL);
		afi_t afi;
		safi_t safi;
		int ret;

		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: activate unknown afi-safi");
			break;
		}

		if (activate)
			ret = peer_activate(peer, afi, safi);
		else
			ret = peer_deactivate(peer, afi, safi);

		/* peer_activate returns BGP_ERR_PEER_SAFI_CONFLICT only when
		 * SAFI_UNICAST conflicts with SAFI_LABELED_UNICAST. lite-YANG
		 * v0.1 exposes only ipv4-unicast, so this is an impossible state
		 * reachable only via mgmtd stale-state replay. Log and continue
		 * rather than returning NB_ERR (APPLY failures can leave the NB
		 * framework in an inconsistent state per northbound.c:2141-2149).
		 */
		if (ret != 0)
			zlog_warn("bgpd-lite: peer_activate/deactivate ret=%d "
				  "(impossible in v0.1, check stale mgmtd state)",
				  ret);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		bool enable = yang_dnode_get_bool(args->dnode, NULL);
		afi_t afi;
		safi_t safi;
		int ret;

		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: soft-reconfig unknown afi-safi");
			break;
		}

		/* Mirror vanilla DEFUN `neighbor X soft-reconfiguration inbound`
		 * (bgp_vty.c): toggle PEER_FLAG_SOFT_RECONFIG (bgpd.h:1887;
		 * 1ULL << 5) via peer_af_flag_set/unset.  Flag enables
		 * Adj-RIB-In storage so subsequent inbound policy changes
		 * can be applied via bgp_soft_reconfig_in (graceful) instead
		 * of full ROUTE-REFRESH. Toggling off frees the stored
		 * Adj-RIB-In via internal cleanup paths.
		 */
		if (enable)
			ret = peer_af_flag_set(peer, afi, safi,
					       PEER_FLAG_SOFT_RECONFIG);
		else
			ret = peer_af_flag_unset(peer, afi, safi,
						 PEER_FLAG_SOFT_RECONFIG);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_af_flag_%s soft-reconfig returned %d",
				  enable ? "set" : "unset", ret);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		const char *name = yang_dnode_get_string(args->dnode, NULL);
		struct route_map *rmap = route_map_lookup_by_name(name);
		int ret;

		/* Mirror vanilla DEFUN `neighbor X route-map NAME in`
		 * (bgp_vty.c:9025..9043) which dispatches via
		 * peer_route_map_set_vty -> route_map_lookup_by_name +
		 * peer_route_map_set(peer, afi, safi, RMAP_IN, name, rmap).
		 * peer_route_map_set XSTRDUPs `name` internally; the YANG
		 * leafref guarantees the rmap exists at commit, so rmap is
		 * non-NULL on this path.  Idempotent on same name (bgpd.c:8161
		 * strcmp short-circuit); peer_on_policy_change is graceful
		 * (route-refresh / soft-reconfig — never session reset).
		 */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: route-map import unknown afi-safi");
			break;
		}
		ret = peer_route_map_set(peer, afi, safi, RMAP_IN,
					 name, rmap);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_route_map_set in returned %d (name=%s)",
				  ret, name);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		int ret;

		/* Mirror vanilla DEFUN `no neighbor X route-map NAME in`
		 * (bgp_vty.c:9051..9070): peer_route_map_unset is NULL-safe
		 * (bgpd.c:8245 NULL check on filter->map[direct].name) and
		 * idempotent on already-unset.
		 */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: route-map import destroy unknown afi-safi");
			break;
		}
		ret = peer_route_map_unset(peer, afi, safi,
					   RMAP_IN);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_route_map_unset in returned %d",
				  ret);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		const char *name = yang_dnode_get_string(args->dnode, NULL);
		struct route_map *rmap = route_map_lookup_by_name(name);
		int ret;

		/* Mirror vanilla `neighbor X route-map NAME out` — same
		 * dispatch as import_modify but with RMAP_OUT direction.
		 * peer_on_policy_change(...,outbound=1) calls
		 * update_group_adjust_peer + bgp_announce_route (graceful
		 * re-announcement; no session reset).
		 */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: route-map export unknown afi-safi");
			break;
		}
		ret = peer_route_map_set(peer, afi, safi, RMAP_OUT,
					 name, rmap);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_route_map_set out returned %d (name=%s)",
				  ret, name);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		int ret;

		/* Mirror vanilla `no neighbor X route-map NAME out`. */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: route-map export destroy unknown afi-safi");
			break;
		}
		ret = peer_route_map_unset(peer, afi, safi,
					   RMAP_OUT);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_route_map_unset out returned %d",
				  ret);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		uint32_t max = yang_dnode_get_uint32(args->dnode, NULL);
		int ret;

		/* Mirror vanilla DEFUN `neighbor X maximum-prefix N`
		 * (bgp_vty.c:9258..9278) which dispatches via
		 * peer_maximum_prefix_set_vty with NULL defaults: vty layer
		 * substitutes MAXIMUM_PREFIX_THRESHOLD_DEFAULT (75; bgpd.h:2124)
		 * for threshold and 0 for warning/restart; force is parsed
		 * from argv and defaults to false here.
		 *
		 * peer_maximum_prefix_set (bgpd.c:8441; 8 args; `warning` is
		 * int) sets PEER_FLAG_MAX_PREFIX + peer->pmax[afi][safi].
		 * On Established peers with `pcount > max` and warning-only
		 * NOT set, calls bgp_maximum_prefix_overflow which sends
		 * BGP_NOTIFY_CEASE_MAX_PREFIX (session reset).  Operators
		 * lowering maximum-prefix on a busy peer must therefore
		 * expect graceful-or-disruptive behavior depending on whether
		 * warning-only is set; v0.1 -lite does not yet expose
		 * warning-only, so lowering on Established triggers Cease
		 * if the stored prefix count exceeds the new limit.  For
		 * non-Established peers the body just stores the limit and
		 * clears overflow state (no disruption).
		 */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: maximum-prefix unknown afi-safi");
			break;
		}
		ret = peer_maximum_prefix_set(peer, afi, safi,
					      max,
					      /* threshold */ MAXIMUM_PREFIX_THRESHOLD_DEFAULT,
					      /* warning   */ 0,
					      /* restart   */ 0,
					      /* force     */ false);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_maximum_prefix_set returned %d (max=%u)",
				  ret, max);
		break;
	}
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
		return lite_afi_safi_validate(args->dnode, args->errmsg, args->errmsg_len);
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct peer *peer = nb_running_get_entry(args->dnode, NULL,
							 true);
		int ret;

		/* Mirror vanilla DEFUN `no neighbor X maximum-prefix`
		 * (bgp_vty.c:9466..9485): peer_maximum_prefix_unset clears
		 * PEER_FLAG_MAX_PREFIX (and FORCE/WARNING) + zeroes
		 * peer->pmax/pmax_threshold/pmax_restart for the AFI/SAFI.
		 */
		afi_t afi;
		safi_t safi;
		if (lite_afi_safi_from_dnode(args->dnode, &afi, &safi) != 0) {
			zlog_warn("bgpd-lite nb: maximum-prefix unset unknown afi-safi");
			break;
		}
		ret = peer_maximum_prefix_unset(peer, afi, safi);
		if (ret != 0)
			zlog_warn("bgpd-lite nb: peer_maximum_prefix_unset returned %d",
				  ret);
		break;
	}
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
	case NB_EV_APPLY: {
		/* No nb_running_set_entry for the network list-entry --
		 * ancestor walk resolves bgp* via instance (skipping the
		 * skeletal address_family entry). See PLAN_4iBV §4. */
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *prefix =
			yang_dnode_get_string(args->dnode, "./prefix");
		int ret = bgp_static_set_simple_ipv4(bgp, prefix);
		/* Wrapper returns -1 on parse error or non-IPv4 family.
		 * YANG canonicalises inet:ipv4-prefix, so failure is
		 * effectively unreachable at runtime. Log and continue --
		 * NB_ERR in APPLY triggers rollback assertions when
		 * nb_running_set_entry was not called (see 4i-BU notes).
		 */
		if (ret != 0)
			zlog_warn("bgpd-lite: bgp_static_set_simple_ipv4(%s) ret=%d",
				  prefix, ret);
		break;
	}
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *prefix =
			yang_dnode_get_string(args->dnode, "./prefix");
		int ret = bgp_static_unset_simple_ipv4(bgp, prefix);
		/* Wrapper returns -1 on parse error, non-IPv4 family, or
		 * route-not-found. Last case is benign (idempotent destroy);
		 * log and continue. */
		if (ret != 0)
			zlog_warn("bgpd-lite: bgp_static_unset_simple_ipv4(%s) ret=%d",
				  prefix, ret);
		break;
	}
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
	case NB_EV_APPLY: {
		/* Same ancestor-walk reasoning as network_create above:
		 * resolve bgp* via instance, skipping the skeletal
		 * address_family entry. */
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *proto_str =
			yang_dnode_get_string(args->dnode, "./protocol");
		uint8_t proto = proto_redistnum(AFI_IP, proto_str);
		int ret;
		if (proto == ZEBRA_ROUTE_ERROR) {
			zlog_warn("bgpd-lite: unknown redistribute protocol '%s'",
				  proto_str);
			break;
		}
		/* Mirror vanilla bgp_vty.c:18820..18840: bgp_redist_add
		 * registers the per-bgp `struct bgp_redist` entry so that
		 * later child-leaf callbacks (e.g., route-map_modify,
		 * metric_modify) can resolve it via bgp_redist_lookup.
		 * bgp_redistribute_set alone updates only the bgp_zclient
		 * bitmap; without bgp_redist_add the per-bgp list stays
		 * empty. This was a latent gap in 4i-BV surfaced by 4i-BW.
		 */
		(void)bgp_redist_add(bgp, AFI_IP, proto, 0);
		ret = bgp_redistribute_set(bgp, AFI_IP, proto, 0, false);
		/* Vanilla returns CMD_WARNING when redistribution flag is
		 * already set (idempotent re-call). Without vty there is no
		 * user-visible side effect; log and continue. */
		if (ret != 0)
			zlog_warn("bgpd-lite: bgp_redistribute_set(%s) ret=%d",
				  proto_str, ret);
		break;
	}
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
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *proto_str =
			yang_dnode_get_string(args->dnode, "./protocol");
		uint8_t proto = proto_redistnum(AFI_IP, proto_str);
		if (proto == ZEBRA_ROUTE_ERROR) {
			zlog_warn("bgpd-lite: unknown redistribute protocol '%s'",
				  proto_str);
			break;
		}
		bgp_redistribute_unset(bgp, AFI_IP, proto, 0);
		break;
	}
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
	case NB_EV_APPLY: {
		/* args->dnode is the leaf "route-map" itself. Ancestor walk
		 * climbs leaf -> redistribute (no entry) -> address-family
		 * (skeletal NO-OP) -> instance (bgp* registered). Sibling key
		 * "protocol" lives on the parent redistribute list-entry.
		 * Mirrors vanilla bgp_vty.c:18820..18840 canonical pattern,
		 * with route_map_lookup_by_name() as the vty-free equivalent
		 * of route_map_lookup_warn_noexist().
		 */
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *proto_str =
			yang_dnode_get_string(args->dnode, "../protocol");
		const char *rmap_name =
			yang_dnode_get_string(args->dnode, NULL);
		uint8_t proto = proto_redistnum(AFI_IP, proto_str);
		struct bgp_redist *red;
		struct route_map *route_map;
		bool changed;

		if (proto == ZEBRA_ROUTE_ERROR) {
			zlog_warn("bgpd-lite: unknown redistribute protocol '%s' in rmap modify",
				  proto_str);
			break;
		}
		red = bgp_redist_lookup(bgp, AFI_IP, proto, 0);
		if (!red) {
			/* redistribute_create must fire before this leaf is
			 * applied. Defensive guard for replay/test corner
			 * cases; safe to no-op (NB_OK keeps rollback quiet,
			 * standing rule 4i-BU §3). */
			zlog_warn("bgpd-lite: redistribute %s not registered; rmap '%s' skipped",
				  proto_str, rmap_name);
			break;
		}
		route_map = route_map_lookup_by_name(rmap_name);
		changed = bgp_redistribute_rmap_set(red, rmap_name, route_map);
		if (changed)
			bgp_redistribute_set(bgp, AFI_IP, proto, 0, true);
		break;
	}
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
	case NB_EV_APPLY: {
		/* Inline mirror of _bgp_redistribute_unset rmap-clear path
		 * (bgp_zebra.c:2455..2457). bgp_redistribute_rmap_set does
		 * not handle NULL name (XSTRDUP would crash), so vanilla
		 * cleans red->rmap directly. Re-applying redistribute with
		 * changed=true forces UPDATE peers without the filter.
		 */
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL, true);
		const char *proto_str =
			yang_dnode_get_string(args->dnode, "../protocol");
		uint8_t proto = proto_redistnum(AFI_IP, proto_str);
		struct bgp_redist *red;

		if (proto == ZEBRA_ROUTE_ERROR) {
			zlog_warn("bgpd-lite: unknown redistribute protocol '%s' on rmap destroy",
				  proto_str);
			break;
		}
		red = bgp_redist_lookup(bgp, AFI_IP, proto, 0);
		if (!red)
			/* redistribute already gone (teardown order). */
			break;
		if (red->rmap.name) {
			XFREE(MTYPE_ROUTE_MAP_NAME, red->rmap.name);
			route_map_counter_decrement(red->rmap.map);
			red->rmap.map = NULL;
			bgp_redistribute_set(bgp, AFI_IP, proto, 0, true);
		}
		break;
	}
	}
	return NB_OK;
}


/* ------------------------------------------------------------------ *
 * 19. .../instance/l2vpn-evpn  (presence container - create / destroy)
 *     ADR-026; session 4i-CH.
 * ------------------------------------------------------------------ */
int frr_bgpd_lite_bgp_instance_l2vpn_evpn_create(struct nb_cb_create_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_l2vpn_evpn_create", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
	case NB_EV_APPLY:
		/* Presence container; no peer/bgp mutation here. The
		 * advertise-all-vni leaf carries the actual runtime side-effect
		 * via its modify callback. nb_running_set_entry is intentionally
		 * NOT called -- ancestor walks resolve to the bgp instance.
		 */
		break;
	}
	return NB_OK;
}

int frr_bgpd_lite_bgp_instance_l2vpn_evpn_destroy(struct nb_cb_destroy_args *args)
{
	BGP_NB_LITE_TRACE("bgp_instance_l2vpn_evpn_destroy", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL,
						       true);

		/* Mirror vanilla `no advertise-all-vni` (bgp_evpn_vty.c:3636
		 * evpn_unset_advertise_all_vni). When the presence container
		 * is removed and advertise_all_vni was previously enabled,
		 * tear down the VNI cache + de-register zebra. NO-OP if the
		 * flag was never set (idempotent destroy).
		 */
		if (bgp->advertise_all_vni) {
			bgp->advertise_all_vni = 0;
			bgp_set_evpn(bgp_get_default());
			bgp_zebra_advertise_all_vni(bgp, 0);
			bgp_evpn_cleanup_on_disable(bgp);
		}
		break;
	}
	}
	return NB_OK;
}

/* ------------------------------------------------------------------ *
 * 20. .../instance/l2vpn-evpn/advertise-all-vni  (modify)
 *     ADR-026; session 4i-CH.
 * ------------------------------------------------------------------ */
int frr_bgpd_lite_bgp_instance_l2vpn_evpn_advertise_all_vni_modify(
	struct nb_cb_modify_args *args)
{
	BGP_NB_LITE_TRACE(
		"bgp_instance_l2vpn_evpn_advertise_all_vni_modify", args);

	switch (args->event) {
	case NB_EV_VALIDATE:
	case NB_EV_PREPARE:
	case NB_EV_ABORT:
		break;
	case NB_EV_APPLY: {
		struct bgp *bgp = nb_running_get_entry(args->dnode, NULL,
						       true);
		bool enable = yang_dnode_get_bool(args->dnode, NULL);

		/* Mirror vanilla DEFUN bgp_evpn_advertise_all_vni
		 * (bgp_evpn_vty.c:3841) which dispatches to
		 * evpn_set_advertise_all_vni (bgp_evpn_vty.c:3625) /
		 * evpn_unset_advertise_all_vni (bgp_evpn_vty.c:3636).
		 * Both helpers are static; we inline their full body here
		 * since lite-YANG does not link bgp_evpn_vty.c symbols.
		 *
		 * Set path: flag = 1; bgp_set_evpn(bgp); register w/ zebra.
		 * Unset path: flag = 0; bgp_set_evpn(bgp_get_default());
		 *             de-register w/ zebra; cleanup VNI cache.
		 */
		if (enable) {
			bgp->advertise_all_vni = 1;
			bgp_set_evpn(bgp);
			bgp_zebra_advertise_all_vni(bgp, 1);
		} else {
			bgp->advertise_all_vni = 0;
			bgp_set_evpn(bgp_get_default());
			bgp_zebra_advertise_all_vni(bgp, 0);
			bgp_evpn_cleanup_on_disable(bgp);
		}
		break;
	}
	}
	return NB_OK;
}
