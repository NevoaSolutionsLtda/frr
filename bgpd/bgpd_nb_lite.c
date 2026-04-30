/*
 * BGP -lite northbound xpath -> callback registry.
 *
 * Author: MGC Connect / Reinaldo Saraiva <reinaldo.saraiva@magalu.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file contains DATA ONLY -- the .nodes[] table that maps each
 * YANG xpath to its callbacks. Follows the staticd/static_nb.c pattern
 * (see poc/frr-grpc-poc/docs/frr_nb_template.md section 3).
 *
 * Registered in bgpd/bgp_main.c::bgpd_yang_modules[] alongside
 * frr_filter_info, frr_route_map_info, frr_vrf_info, frr_interface_info.
 */

#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"

#include "bgpd/bgpd_nb_lite.h"

/* clang-format off */
const struct frr_yang_module_info frr_bgpd_lite_info = {
	.name = "frr-bgpd-lite",
	.nodes = {
		/* 1. instance */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_create,
				.destroy = frr_bgpd_lite_bgp_instance_destroy,
			},
		},
		/* 2. router-id */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/router-id",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_router_id_modify,
				.destroy = frr_bgpd_lite_bgp_instance_router_id_destroy,
			},
		},
		/* 3. as-path-multipath-relax */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/as-path-multipath-relax",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_as_path_multipath_relax_modify,
			},
		},
		/* 4. ebgp-requires-policy */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/ebgp-requires-policy",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_ebgp_requires_policy_modify,
			},
		},
		/* 5. network-import-check */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/network-import-check",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_network_import_check_modify,
			},
		},
		/* 6. neighbor */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_neighbor_create,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_destroy,
			},
		},
		/* 7. neighbor/remote-as */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/remote-as",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_remote_as_modify,
			},
		},
		/* 8. neighbor/bfd (leaf with default -- no destroy) */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/bfd",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_bfd_modify,
			},
		},
		/* 9. neighbor/description */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/description",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_neighbor_description_modify,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_description_destroy,
			},
		},
		/* 10. neighbor/timers (container -- apply_finish for atomicity) */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/timers",
			.cbs = {
				.apply_finish = frr_bgpd_lite_bgp_instance_neighbor_timers_apply_finish,
			},
		},
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/timers/keepalive",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_timers_keepalive_modify,
			},
		},
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/timers/hold-time",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_timers_hold_time_modify,
			},
		},
		/* 11. neighbor/address-family */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_neighbor_address_family_create,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_address_family_destroy,
			},
		},
		/* 12. neighbor/address-family/activate */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family/activate",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_address_family_activate_modify,
			},
		},
		/* 13. neighbor/address-family/soft-reconfiguration-inbound */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family/soft-reconfiguration-inbound",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_neighbor_address_family_soft_reconfiguration_inbound_modify,
			},
		},
		/* 14. neighbor/address-family/route-map/import */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family/route-map/import",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_modify,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_import_destroy,
			},
		},
		/* 15. neighbor/address-family/route-map/export */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family/route-map/export",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_modify,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_address_family_route_map_export_destroy,
			},
		},
		/* 16. neighbor/address-family/maximum-prefix */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/neighbor/address-family/maximum-prefix",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_modify,
				.destroy = frr_bgpd_lite_bgp_instance_neighbor_address_family_maximum_prefix_destroy,
			},
		},
		/* 17. address-family (global list -- stubs) */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/address-family",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_address_family_create,
				.destroy = frr_bgpd_lite_bgp_instance_address_family_destroy,
			},
		},
		/* 17a. address-family/network */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/address-family/network",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_address_family_network_create,
				.destroy = frr_bgpd_lite_bgp_instance_address_family_network_destroy,
			},
		},
		/* 18. address-family/redistribute */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/address-family/redistribute",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_address_family_redistribute_create,
				.destroy = frr_bgpd_lite_bgp_instance_address_family_redistribute_destroy,
			},
		},
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/address-family/redistribute/route-map",
			.cbs = {
				.modify  = frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_modify,
				.destroy = frr_bgpd_lite_bgp_instance_address_family_redistribute_route_map_destroy,
			},
		},
		/* 19. l2vpn-evpn (presence container) */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/l2vpn-evpn",
			.cbs = {
				.create  = frr_bgpd_lite_bgp_instance_l2vpn_evpn_create,
				.destroy = frr_bgpd_lite_bgp_instance_l2vpn_evpn_destroy,
			},
		},
		/* 20. l2vpn-evpn/advertise-all-vni */
		{
			.xpath = "/frr-bgpd-lite:bgp/instance/l2vpn-evpn/advertise-all-vni",
			.cbs = {
				.modify = frr_bgpd_lite_bgp_instance_l2vpn_evpn_advertise_all_vni_modify,
			},
		},
		/* sentinel */
		{
			.xpath = NULL,
		},
	},
};
/* clang-format on */

/* TODO(mgc-connect): implement all callbacks referenced above -- see
 * bgpd_nb_config_lite.c and bgpd_lite_design.md section 3 for the
 * xpath/LOC map.
 */
