/*
 * BGP -lite northbound callback declarations.
 *
 * Author: MGC Connect / Reinaldo Saraiva <reinaldo.saraiva@magalu.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Staged in poc/frr-grpc-poc/fork-staging/bgpd-lite/ for later copy
 * into <frr>/bgpd/. Follows the 4-file northbound pattern established
 * by staticd/static_nb.h and OSPF -lite (FRR PR #18401).
 */

#ifndef _FRR_BGPD_NB_LITE_H_
#define _FRR_BGPD_NB_LITE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * YANG module descriptor -- registered in bgpd/bgp_main.c::bgpd_di
 * (see bgpd_lite_design.md section 4).
 */
extern const struct frr_yang_module_info frr_bgpd_lite_info;

/* ------------------------------------------------------------------ *
 * Config callbacks -- implemented in bgpd_nb_config_lite.c
 *
 * Naming convention: <xpath with '/' -> '_' and '-' -> '_'>_<op>.
 * ------------------------------------------------------------------ */

/* 1. /frr-bgpd-lite:bgp/instance (create / destroy) */
int frr_bgpd_lite_bgp_instance_create(struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_destroy(struct nb_cb_destroy_args *args);

/* 2. .../instance/router-id (modify / destroy) */
int frr_bgpd_lite_bgp_instance_router_id_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_router_id_destroy(
	struct nb_cb_destroy_args *args);

/* 3. .../instance/as-path-multipath-relax (modify) */
int frr_bgpd_lite_bgp_instance_as_path_multipath_relax_modify(
	struct nb_cb_modify_args *args);

/* 4. .../instance/ebgp-requires-policy (modify) */
int frr_bgpd_lite_bgp_instance_ebgp_requires_policy_modify(
	struct nb_cb_modify_args *args);

/* 5. .../instance/network-import-check (modify) */
int frr_bgpd_lite_bgp_instance_network_import_check_modify(
	struct nb_cb_modify_args *args);

/* 6. .../instance/neighbor (create / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_create(
	struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_destroy(
	struct nb_cb_destroy_args *args);

/* 7. .../neighbor/remote-as (modify) */
int frr_bgpd_lite_bgp_instance_neighbor_remote_as_modify(
	struct nb_cb_modify_args *args);

/* 8. .../neighbor/bfd (modify / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_bfd_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_bfd_destroy(
	struct nb_cb_destroy_args *args);

/* 9. .../neighbor/description (modify / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_description_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_description_destroy(
	struct nb_cb_destroy_args *args);

/* 10. .../neighbor/timers (apply_finish + per-leaf modify) */
int frr_bgpd_lite_bgp_instance_neighbor_timers_keepalive_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_timers_hold_time_modify(
	struct nb_cb_modify_args *args);
void frr_bgpd_lite_bgp_instance_neighbor_timers_apply_finish(
	struct nb_cb_apply_finish_args *args);

/* 11. .../neighbor/address-family (create / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_create(
	struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_address_family_destroy(
	struct nb_cb_destroy_args *args);

/* 12. .../neighbor/address-family/activate (modify) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_activate_modify(
	struct nb_cb_modify_args *args);

/* 13. .../neighbor/address-family/soft-reconfiguration-inbound (modify) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_soft_reconfiguration_inbound_modify(
	struct nb_cb_modify_args *args);

/* 14. .../neighbor/address-family/route-map/import (modify / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_destroy(
	struct nb_cb_destroy_args *args);

/* 15. .../neighbor/address-family/route-map/export (modify / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_destroy(
	struct nb_cb_destroy_args *args);

/* 16. .../neighbor/address-family/maximum-prefix (modify / destroy) */
int frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_destroy(
	struct nb_cb_destroy_args *args);

/* 17. .../address-family (global list container - stubs) */
int frr_bgpd_lite_bgp_instance_address_family_create(
	struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_address_family_destroy(
	struct nb_cb_destroy_args *args);

/* 17a. .../address-family/network (create / destroy) */
int frr_bgpd_lite_bgp_instance_address_family_network_create(
	struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_address_family_network_destroy(
	struct nb_cb_destroy_args *args);

/* 18. .../address-family/redistribute (create / destroy + route-map) */
int frr_bgpd_lite_bgp_instance_address_family_redistribute_create(
	struct nb_cb_create_args *args);
int frr_bgpd_lite_bgp_instance_address_family_redistribute_destroy(
	struct nb_cb_destroy_args *args);
int frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_modify(
	struct nb_cb_modify_args *args);
int frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_destroy(
	struct nb_cb_destroy_args *args);

/* ------------------------------------------------------------------ *
 * State callbacks -- implemented in bgpd_nb_state_lite.c
 *
 * All 15 writable leaves above are config-true; there are no oper-only
 * getters in -lite v0.1. Peer operational data is served by the
 * separate frr-bgp-peer module (FRR PR #21297).
 * ------------------------------------------------------------------ */

/* (none) */

#ifdef __cplusplus
}
#endif

#endif /* _FRR_BGPD_NB_LITE_H_ */
