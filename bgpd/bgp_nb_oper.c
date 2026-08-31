// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026        Nevoa Solutions Ltda.
 */
#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"
#include "lib_errors.h"
#include "log.h"
#include "monotime.h"
#include "sockunion.h"
#include "vrf.h"
#include "routing_nb.h"

#include "bgpd.h"
#include "bgp_table.h"
#include "bgp_debug.h"
#include "bgp_fsm.h"
#include "bgp_nb.h"
#include "bgp_nb_oper.h"

/*
 * B5.1: bgpd operational state.
 *
 * The frr-bgp-oper module is served in tree mode: one private libyang
 * tree is built per oper walk (module-level get_tree_locked below)
 * from the live struct bgp/struct peer objects, and every node of the
 * module resolves against it (lib/northbound_oper.c). The private
 * per-walk tree sidesteps yield-vs-mutation staleness: oper walks
 * yield across event loop iterations while bgpd keeps mutating peers.
 *
 * The two trunk lists the state sits under stay in callback mode
 * (tree mode is module-wide): the control-plane-protocol list
 * (frr-routing, served through the per-daemon hooks declared in
 * lib/routing_nb.h) and the neighbors/neighbor list (frr-bgp, the
 * callbacks registered from bgpd/bgp_nb.c and implemented here).
 * Their key values must stay identical to the paths this builder
 * materializes: both sides derive from bgp_nb_cpp_name()/
 * bgp_nb_vrf_key() and the peer remote sockunion.
 */

#define BGP_NB_OPER_CPP_BASE                                                   \
	"/frr-routing:routing/control-plane-protocols/"                        \
	"control-plane-protocol[type='frr-bgp:bgp'][name='%s'][vrf='%s']"

/* Instances served on the control-plane-protocol trunk list. */
static bool bgp_nb_oper_instance_served(const struct bgp *bgp)
{
	return !IS_BGP_INSTANCE_HIDDEN(bgp);
}

/* Neighbors served on the neighbors/neighbor trunk list: the yang key
 * is remote-address, so interface-based (unnumbered) peers and peers
 * without a known remote address have no list entry.
 */
static bool bgp_nb_oper_peer_served(const struct peer *peer)
{
	const union sockunion *su_remote = peer->connection->su_remote;

	if (peer->conf_if)
		return false;
	if (!su_remote)
		return false;
	if (su_remote->sa.sa_family != AF_INET &&
	    su_remote->sa.sa_family != AF_INET6)
		return false;
	return true;
}

static const char *bgp_nb_oper_peer_key(const struct peer *peer,
					char *buf, size_t buflen)
{
	return sockunion2str(peer->connection->su_remote, buf, buflen);
}

/* Address families with state to report for the instance. */
static bool bgp_nb_oper_af_served(const struct bgp *bgp, afi_t afi,
				  safi_t safi)
{
	struct peer *peer;
	struct listnode *node;

	if (safi == SAFI_UNREACH)
		return false;
	if (bgp_table_count(bgp->rib[afi][safi]))
		return true;
	for (ALL_LIST_ELEMENTS_RO(bgp->peer, node, peer))
		if (peer->afc[afi][safi])
			return true;
	return false;
}

/* Add one leaf (value string) at an absolute path rooted at *treep.
 * The tree root is captured from the first created node (lyd_new_path
 * returns the FIRST created node, so climb to the top).
 */
static void bgp_nb_oper_add_abs(struct lyd_node **treep, const char *value,
				const char *fmt, ...)
{
	char path[XPATH_MAXLEN];
	struct lyd_node *node = NULL;
	va_list ap;

	if (!value)
		return;

	va_start(ap, fmt);
	vsnprintfrr(path, sizeof(path), fmt, ap);
	va_end(ap);

	if (lyd_new_path(*treep, ly_native_ctx, path, value, 0, &node)) {
		flog_warn(EC_LIB_LIBYANG, "%s: lyd_new_path(%s) failed: %s",
			  __func__, path, ly_last_errmsg());
		return;
	}
	if (!*treep && node) {
		*treep = node;
		while ((*treep)->parent)
			*treep = lyd_parent(*treep);
	}
}

static void bgp_nb_oper_build_instance(struct lyd_node **treep,
					const char *base, const struct bgp *bgp)
{
	char addrbuf[SU_ADDRSTRLEN];
	char numbuf[64];
	struct peer *peer;
	struct listnode *node;
	afi_t afi;
	safi_t safi;

	/* instance state: router-id, local-as, view */
	inet_ntop(AF_INET, &bgp->router_id, addrbuf, sizeof(addrbuf));
	bgp_nb_oper_add_abs(treep, addrbuf, "%s/frr-bgp-oper:state/router-id",
			    base);
	snprintfrr(numbuf, sizeof(numbuf), "%u", bgp->as);
	bgp_nb_oper_add_abs(treep, numbuf, "%s/frr-bgp-oper:state/local-as",
			    base);
	if (bgp->inst_type == BGP_INSTANCE_TYPE_VIEW && bgp->name)
		bgp_nb_oper_add_abs(treep, bgp->name,
				    "%s/frr-bgp-oper:state/view", base);

	/* global per-afi-safi counters */
	FOREACH_AFI_SAFI (afi, safi) {
		const char *af_name;

		if (!bgp_nb_oper_af_served(bgp, afi, safi))
			continue;
		af_name = bgp_nb_af_yang_name(afi, safi);
		if (!af_name)
			continue;

		snprintfrr(numbuf, sizeof(numbuf), "%" PRIu64,
			   bgp_table_version(bgp->rib[afi][safi]));
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/global/frr-bgp-oper:state/afi-safi[afi-safi-name='%s']/table-version",
			base, af_name);
		snprintfrr(numbuf, sizeof(numbuf), "%lu",
			   bgp_table_count(bgp->rib[afi][safi]));
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/global/frr-bgp-oper:state/afi-safi[afi-safi-name='%s']/rib-count",
			base, af_name);
	}

	/* neighbor session state */
	for (ALL_LIST_ELEMENTS_RO(bgp->peer, node, peer)) {
		const char *peer_key;
		const char *fsm_state;
		struct peer_af *paf;
		safi_t pfx_rcd_safi;
		uint64_t uptime_msec;

		if (!bgp_nb_oper_peer_served(peer))
			continue;
		peer_key = bgp_nb_oper_peer_key(peer, addrbuf,
						sizeof(addrbuf));

		fsm_state = lookup_msg(bgp_status_msg,
				       peer->connection->status, NULL);
		bgp_nb_oper_add_abs(
			treep, fsm_state,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/fsm-state",
			base, peer_key);
		snprintfrr(numbuf, sizeof(numbuf), "%u", peer->established);
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/connections-established",
			base, peer_key);
		snprintfrr(numbuf, sizeof(numbuf), "%u", peer->dropped);
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/connections-dropped",
			base, peer_key);
		uptime_msec = peer->uptime
				      ? (uint64_t)(monotime(NULL) -
						   peer->uptime) * 1000
				      : 0;
		snprintfrr(numbuf, sizeof(numbuf), "%" PRIu64, uptime_msec);
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/uptime-msec",
			base, peer_key);
		bgp_nb_oper_add_abs(
			treep, peer_down_str[(int)peer->last_reset],
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/last-reset",
			base, peer_key);
		snprintfrr(numbuf, sizeof(numbuf), "%" PRIu64,
			   (uint64_t)PEER_TOTAL_RX(peer));
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/msg-rcvd",
			base, peer_key);
		snprintfrr(numbuf, sizeof(numbuf), "%" PRIu64,
			   (uint64_t)PEER_TOTAL_TX(peer));
		bgp_nb_oper_add_abs(
			treep, numbuf,
			"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/msg-snt",
			base, peer_key);

		FOREACH_AFI_SAFI (afi, safi) {
			const char *af_name;

			if (!peer->afc[afi][safi])
				continue;
			af_name = bgp_nb_af_yang_name(afi, safi);
			if (!af_name)
				continue;

			/* pfxRcd of labeled-unicast lives in the unicast
			 * table (same accounting as show bgp summary).
			 */
			if (safi == SAFI_LABELED_UNICAST)
				pfx_rcd_safi = SAFI_UNICAST;
			else
				pfx_rcd_safi = safi;
			paf = peer_af_find((struct peer *)peer, afi, safi);

			snprintfrr(numbuf, sizeof(numbuf), "%u",
				   peer->pcount[afi][pfx_rcd_safi]);
			bgp_nb_oper_add_abs(
				treep, numbuf,
				"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/afi-safi[afi-safi-name='%s']/pfx-rcd",
				base, peer_key, af_name);
			snprintfrr(numbuf, sizeof(numbuf), "%u",
				   (paf && PAF_SUBGRP(paf))
					   ? PAF_SUBGRP(paf)->scount
					   : 0);
			bgp_nb_oper_add_abs(
				treep, numbuf,
				"%s/neighbors/neighbor[remote-address='%s']/frr-bgp-oper:state/afi-safi[afi-safi-name='%s']/pfx-snt",
				base, peer_key, af_name);
		}
	}
}

static struct lyd_node *bgp_nb_oper_build_tree(void)
{
	struct lyd_node *tree = NULL;
	struct bgp *bgp;
	struct listnode *node, *nnode;
	char base[XPATH_MAXLEN];

	for (ALL_LIST_ELEMENTS(bm->bgp, node, nnode, bgp)) {
		if (!bgp_nb_oper_instance_served(bgp))
			continue;

		snprintfrr(base, sizeof(base),
			   BGP_NB_OPER_CPP_BASE "/frr-bgp:bgp",
			   bgp_nb_cpp_name(bgp), bgp_nb_vrf_key(bgp));
		bgp_nb_oper_build_instance(&tree, base, bgp);
	}

	return tree;
}

static const struct lyd_node *bgp_nb_oper_get_tree_locked(
	const char *xpath __attribute__((unused)), void **user_lock)
{
	struct lyd_node *tree = bgp_nb_oper_build_tree();

	/* Single-threaded walk context; the lock handle only carries the
	 * tree for the matching unlock_tree.
	 */
	*user_lock = tree;
	return tree;
}

static void bgp_nb_oper_unlock_tree(const struct lyd_node *tree,
				    void *user_lock __attribute__((unused)))
{
	if (tree)
		lyd_free_all((struct lyd_node *)tree);
}

/* clang-format off */
const struct frr_yang_module_info frr_bgp_oper_info = {
	.name = "frr-bgp-oper",
	.get_tree_locked = bgp_nb_oper_get_tree_locked,
	.unlock_tree = bgp_nb_oper_unlock_tree,
	.nodes = {
		{
			.xpath = NULL,
		},
	}
};
/* clang-format on */

/*
 * Trunk list: /frr-bgp:bgp/neighbors/neighbor (key: remote-address).
 * The opaque list entries are struct peer pointers of the parent
 * instance (the control-plane-protocol list entry).
 */
const void *bgp_nb_oper_neighbor_get_next(struct nb_cb_get_next_args *args)
{
	struct bgp *bgp = args->parent_list_entry;
	struct listnode *node;
	struct peer *peer;

	if (!bgp)
		return NULL;

	if (args->list_entry) {
		bool seen = false;

		for (ALL_LIST_ELEMENTS_RO(bgp->peer, node, peer)) {
			if (!bgp_nb_oper_peer_served(peer))
				continue;
			if (seen)
				return peer;
			if (peer == args->list_entry)
				seen = true;
		}
		return NULL;
	}

	for (ALL_LIST_ELEMENTS_RO(bgp->peer, node, peer))
		if (bgp_nb_oper_peer_served(peer))
			return peer;
	return NULL;
}

int bgp_nb_oper_neighbor_get_keys(struct nb_cb_get_keys_args *args)
{
	const struct peer *peer = args->list_entry;
	char addrbuf[SU_ADDRSTRLEN];

	args->keys->num = 1;
	strlcpy(args->keys->key[0],
		bgp_nb_oper_peer_key(peer, addrbuf, sizeof(addrbuf)),
		sizeof(args->keys->key[0]));

	return NB_OK;
}

const void *bgp_nb_oper_neighbor_lookup_entry(
	struct nb_cb_lookup_entry_args *args)
{
	struct bgp *bgp = args->parent_list_entry;
	struct listnode *node;
	struct peer *peer;
	char addrbuf[SU_ADDRSTRLEN];

	if (!bgp || args->keys->num != 1)
		return NULL;

	for (ALL_LIST_ELEMENTS_RO(bgp->peer, node, peer)) {
		if (!bgp_nb_oper_peer_served(peer))
			continue;
		if (strmatch(bgp_nb_oper_peer_key(peer, addrbuf,
						  sizeof(addrbuf)),
			     args->keys->key[0]))
			return peer;
	}
	return NULL;
}

/*
 * Trunk list: /frr-routing:.../control-plane-protocol (keys: type,
 * name, vrf). The opaque list entries are struct bgp pointers; lib
 * stays daemon-agnostic through the routing_nb.h hooks and bgpd is
 * currently their only subscriber.
 */
static int bgp_nb_oper_cpp_next(const void **entry)
{
	struct listnode *node, *nnode;
	struct bgp *bgp;

	if (*entry) {
		bool seen = false;

		for (ALL_LIST_ELEMENTS(bm->bgp, node, nnode, bgp)) {
			if (!bgp_nb_oper_instance_served(bgp))
				continue;
			if (seen) {
				*entry = bgp;
				return 1;
			}
			if (bgp == *entry)
				seen = true;
		}
		return 0;
	}

	for (ALL_LIST_ELEMENTS(bm->bgp, node, nnode, bgp)) {
		if (!bgp_nb_oper_instance_served(bgp))
			continue;
		*entry = bgp;
		return 1;
	}
	return 0;
}

static int bgp_nb_oper_cpp_keys(const void *entry, struct yang_list_keys *keys)
{
	const struct bgp *bgp = entry;

	keys->num = 3;
	strlcpy(keys->key[0], "frr-bgp:bgp", sizeof(keys->key[0]));
	strlcpy(keys->key[1], bgp_nb_cpp_name(bgp), sizeof(keys->key[1]));
	strlcpy(keys->key[2], bgp_nb_vrf_key(bgp), sizeof(keys->key[2]));

	return 1;
}

static int bgp_nb_oper_cpp_lookup(const struct yang_list_keys *keys,
				  const void **entry)
{
	struct listnode *node, *nnode;
	struct bgp *bgp;

	if (keys->num != 3 || !strmatch(keys->key[0], "frr-bgp:bgp"))
		return 0;

	for (ALL_LIST_ELEMENTS(bm->bgp, node, nnode, bgp)) {
		if (!bgp_nb_oper_instance_served(bgp))
			continue;
		if (strmatch(bgp_nb_cpp_name(bgp), keys->key[1]) &&
		    strmatch(bgp_nb_vrf_key(bgp), keys->key[2])) {
			*entry = bgp;
			return 1;
		}
	}
	return 0;
}

void bgp_nb_oper_init(void)
{
	hook_register(routing_control_plane_protocol_oper_next,
		      bgp_nb_oper_cpp_next);
	hook_register(routing_control_plane_protocol_oper_keys,
		      bgp_nb_oper_cpp_keys);
	hook_register(routing_control_plane_protocol_oper_lookup,
		      bgp_nb_oper_cpp_lookup);
}
