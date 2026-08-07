// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * BGP northbound — module registration.
 *
 * Copyright (C) 2026 FRRouting
 */

#include <zebra.h>

#include "lib/command.h"
#include "lib/log.h"
#include "lib/northbound.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_nb.h"
#include "bgpd/bgp_nb_stubs.h"

/* clang-format off */
const struct frr_yang_module_info frr_bgp_info = {
	.name = "frr-bgp",
	.nodes = {
#include "bgpd/bgp_nb_stubs_table.inc"

		/* control-plane-protocol */
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp",
			.cbs = {
				.create  = bgp_router_create,
				.destroy = bgp_router_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},

		/* global */
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/router-id",
			.cbs = {
				.modify   = bgp_global_router_id_modify,
				.destroy  = bgp_global_router_id_destroy,
				.cli_show = bgp_global_router_id_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/default-shutdown",
			.cbs = {
				.modify   = bgp_global_default_shutdown_modify,
				.cli_show = bgp_global_default_shutdown_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/show-hostname",
			.cbs = {
				.modify  = bgp_global_show_hostname_modify,
				.cli_show = bgp_global_show_hostname_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/show-nexthop-hostname",
			.cbs = {
				.modify  = bgp_global_show_nexthop_hostname_modify,
				.cli_show = bgp_global_show_nexthop_hostname_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/always-compare-med",
			.cbs = {
				.modify  = bgp_global_always_compare_med_modify,
				.cli_show = bgp_global_always_compare_med_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/external-compare-router-id",
			.cbs = {
				.modify  = bgp_global_external_compare_router_id_modify,
				.cli_show = bgp_global_external_compare_router_id_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/ignore-as-path-length",
			.cbs = {
				.modify  = bgp_global_ignore_as_path_length_modify,
				.cli_show = bgp_global_ignore_as_path_length_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/aspath-confed",
			.cbs = {
				.modify  = bgp_global_aspath_confed_modify,
				.cli_show = bgp_global_aspath_confed_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/confed-med",
			.cbs = {
				.modify  = bgp_global_confed_med_modify,
				.cli_show = bgp_global_confed_med_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/missing-as-worst-med",
			.cbs = {
				.modify  = bgp_global_missing_as_worst_med_modify,
				.cli_show = bgp_global_missing_as_worst_med_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-neighbor-config/log-neighbor-changes",
			.cbs = {
				.modify   = bgp_global_log_neighbor_changes_modify,
				.cli_show = bgp_global_log_neighbor_changes_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/import-check",
			.cbs = {
				.modify  = bgp_global_import_check_modify,
				.cli_show = bgp_global_import_check_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-neighbor-config/packet-quanta-config/wpkt-quanta",
			.cbs = {
				.modify  = bgp_global_wpkt_quanta_modify,
				.cli_show = bgp_global_wpkt_quanta_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-neighbor-config/packet-quanta-config/rpkt-quanta",
			.cbs = {
				.modify  = bgp_global_rpkt_quanta_modify,
				.cli_show = bgp_global_rpkt_quanta_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-update-group-config/coalesce-time",
			.cbs = {
				.modify  = bgp_global_coalesce_time_modify,
				.cli_show = bgp_global_coalesce_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-update-group-config/subgroup-pkt-queue-size",
			.cbs = {
				.modify  = bgp_global_subgroup_pkt_queue_size_modify,
				.cli_show = bgp_global_subgroup_pkt_queue_size_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/confederation/identifier",
			.cbs = {
				.modify  = bgp_global_confederation_identifier_modify,
				.destroy = bgp_global_confederation_identifier_destroy,
				.cli_show = bgp_global_confederation_identifier_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/confederation/member-as",
			.cbs = {
				.create  = bgp_global_confederation_member_as_create,
				.destroy = bgp_global_confederation_member_as_destroy,
				.cli_show = bgp_global_confederation_member_as_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/minimum-holdtime",
			.cbs = {
				.modify  = bgp_global_minimum_holdtime_modify,
				.destroy = bgp_global_minimum_holdtime_destroy,
				.cli_show = bgp_global_minimum_holdtime_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/allow-martian-nexthop",
			.cbs = {
				.modify   = bgp_global_allow_martian_nexthop_modify,
				.cli_show = bgp_global_allow_martian_nexthop_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/use-underlays-nexthop-weight",
			.cbs = {
				.modify  = bgp_global_use_underlays_nexthop_weight_modify,
				.cli_show = bgp_global_use_underlays_nexthop_weight_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-reflector/allow-outbound-policy",
			.cbs = {
				.modify  = bgp_global_route_reflector_allow_outbound_policy_modify,
				.cli_show = bgp_global_allow_outbound_policy_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/afi-safis/afi-safi/link-state/distribute/bgp-fabric-link-state",
			.cbs = {
				.create  = bgp_global_bgp_ls_distribute_create,
				.destroy = bgp_global_bgp_ls_distribute_destroy,
				.cli_show = bgp_global_bgp_ls_distribute_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/afi-safis/afi-safi/link-state/distribute/bgp-fabric-link-state/instance-id",
			.cbs = {
				.modify = bgp_global_bgp_ls_distribute_instance_id_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
#define BGP_NB_GLOBAL_AF_XPATH(_af, _leaf)                                     \
	"/frr-routing:routing/control-plane-protocols/control-plane-protocol/" \
	"frr-bgp:bgp/global/afi-safis/afi-safi/" _af "/" _leaf
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH("ipv4-unicast",
						  "redistribution-list"),
		  .cbs = {
			  .create = bgp_global_af_redistribution_list_create,
			  .destroy = bgp_global_af_redistribution_list_destroy,
			  .cli_show = bgp_global_af_redistribution_list_cli_show,
		  } },
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH("ipv4-unicast",
						  "redistribution-list/metric"),
		  .cbs = {
			  .modify = bgp_global_af_redistribution_metric_modify,
			  .destroy = bgp_global_af_redistribution_metric_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH(
			  "ipv4-unicast",
			  "redistribution-list/rmap-policy-import"),
		  .cbs = {
			  .modify = bgp_global_af_redistribution_rmap_modify,
			  .destroy = bgp_global_af_redistribution_rmap_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH("ipv6-unicast",
						  "redistribution-list"),
		  .cbs = {
			  .create = bgp_global_af_redistribution_list_create,
			  .destroy = bgp_global_af_redistribution_list_destroy,
			  .cli_show = bgp_global_af_redistribution_list_cli_show,
		  } },
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH("ipv6-unicast",
						  "redistribution-list/metric"),
		  .cbs = {
			  .modify = bgp_global_af_redistribution_metric_modify,
			  .destroy = bgp_global_af_redistribution_metric_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_GLOBAL_AF_XPATH(
			  "ipv6-unicast",
			  "redistribution-list/rmap-policy-import"),
		  .cbs = {
			  .modify = bgp_global_af_redistribution_rmap_modify,
			  .destroy = bgp_global_af_redistribution_rmap_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
#undef BGP_NB_GLOBAL_AF_XPATH
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/compare-aigp",
			.cbs = {
				.modify  = bgp_global_bestpath_aigp_modify,
				.cli_show = bgp_global_bestpath_aigp_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/use-imported-attributes",
			.cbs = {
				.modify  = bgp_global_bestpath_use_imported_attributes_modify,
				.cli_show = bgp_global_bestpath_use_imported_attributes_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-neighbor-config/dynamic-neighbors-limit",
			.cbs = {
				.modify  = bgp_global_dynamic_neighbors_limit_modify,
				.destroy = bgp_global_dynamic_neighbors_limit_destroy,
				.cli_show = bgp_global_dynamic_neighbors_limit_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/med-config",
			.cbs = {
				.apply_finish = bgp_global_med_config_apply_finish,
				.cli_show = bgp_global_med_config_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/default-software-version-capability",
			.cbs = {
				.modify  = bgp_global_default_software_version_capability_modify,
				.cli_show = bgp_global_default_software_version_capability_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/tcp-keepalive",
			.cbs = {
				.apply_finish = bgp_global_tcp_keepalive_apply_finish,
				.cli_show = bgp_global_tcp_keepalive_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/hold-time",
			.cbs = {
				.modify  = bgp_global_hold_time_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/keepalive",
			.cbs = {
				.modify  = bgp_global_keepalive_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/reject-as-sets",
			.cbs = {
				.modify  = bgp_global_reject_as_sets_modify,
				.cli_show = bgp_global_reject_as_sets_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/enabled",
			.cbs = {
				.modify  = bgp_global_graceful_restart_enabled_modify,
				.destroy = bgp_global_graceful_restart_enabled_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/restart-time",
			.cbs = {
				.modify  = bgp_global_graceful_restart_restart_time_modify,
				.cli_show = bgp_global_restart_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/selection-deferral-time",
			.cbs = {
				.modify  = bgp_global_graceful_restart_selection_deferral_time_modify,
				.cli_show = bgp_global_selection_deferral_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/shutdown",
			.cbs = {
				.modify   = bgp_global_shutdown_modify,
				.cli_show = bgp_global_shutdown_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/shutdown-message",
			.cbs = {
				.modify   = bgp_global_shutdown_message_modify,
				.destroy  = bgp_global_shutdown_message_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/enforce-first-as",
			.cbs = {
				.modify  = bgp_global_enforce_first_as_global_modify,
				.destroy = bgp_global_enforce_first_as_global_destroy,
				.cli_show = bgp_global_enforce_first_as_global_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/suppress-duplicates",
			.cbs = {
				.modify  = bgp_global_suppress_duplicates_modify,
				.cli_show = bgp_global_suppress_duplicates_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/ebgp-requires-policy",
			.cbs = {
				.modify  = bgp_global_ebgp_requires_policy_modify,
				.cli_show = bgp_global_ebgp_requires_policy_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/fast-external-failover",
			.cbs = {
				.modify  = bgp_global_fast_external_failover_modify,
				.cli_show = bgp_global_fast_external_failover_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/deterministic-med",
			.cbs = {
				.modify  = bgp_global_deterministic_med_modify,
				.cli_show = bgp_global_deterministic_med_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/labeled-unicast-explicit-null",
			.cbs = {
				.modify  = bgp_global_labeled_unicast_explicit_null_modify,
				.cli_show = bgp_global_labeled_unicast_explicit_null_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/ipv6-auto-ra",
			.cbs = {
				.modify  = bgp_global_ipv6_auto_ra_modify,
				.cli_show = bgp_global_ipv6_auto_ra_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/allow-multiple-as",
			.cbs = {
				.modify  = bgp_global_allow_multiple_as_modify,
				.cli_show = bgp_global_allow_multiple_as_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/multi-path-as-set",
			.cbs = {
				.modify  = bgp_global_multi_path_as_set_modify,
				.destroy = bgp_global_multi_path_as_set_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/peer-type-multipath-relax",
			.cbs = {
				.modify  = bgp_global_peer_type_multipath_relax_modify,
				.cli_show = bgp_global_peer_type_multipath_relax_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-shutdown/enable",
			.cbs = {
				.modify  = bgp_global_graceful_shutdown_enable_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/suppress-fib-pending",
			.cbs = {
				.modify   = bgp_global_suppress_fib_pending_modify,
				.cli_show = bgp_global_suppress_fib_pending_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/suppress-fib-pending-delay",
			.cbs = {
				.modify   = bgp_global_suppress_fib_pending_delay_modify,
				.destroy  = bgp_global_suppress_fib_pending_delay_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/advertisement-delay-time",
			.cbs = {
				.modify  = bgp_global_advertisement_delay_global_modify,
				.destroy = bgp_global_advertisement_delay_global_destroy,
				.cli_show = bgp_global_advertisement_delay_global_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/update-delay-time",
			.cbs = {
				.modify  = bgp_global_update_delay_time_modify,
				.destroy = bgp_global_update_delay_time_destroy,
				.cli_show = bgp_global_update_delay_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/establish-wait-time",
			.cbs = {
				.modify  = bgp_global_establish_wait_time_modify,
				.destroy = bgp_global_establish_wait_time_destroy,
				.cli_show = bgp_global_establish_wait_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/connect-retry-interval",
			.cbs = {
				.modify  = bgp_global_connect_retry_interval_modify,
				.cli_show = bgp_global_connect_retry_interval_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/conditional-advertisement-timer",
			.cbs = {
				.modify  = bgp_global_conditional_advertisement_period_modify,
				.cli_show = bgp_global_conditional_advertisement_period_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/global-config-timers/default-originate-timer",
			.cbs = {
				.modify  = bgp_global_default_originate_timer_modify,
				.destroy = bgp_global_default_originate_timer_destroy,
				.cli_show = bgp_global_default_originate_timer_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-selection-options/bandwidth-handling",
			.cbs = {
				.modify  = bgp_global_bestpath_bandwidth_modify,
				.cli_show = bgp_global_bestpath_bandwidth_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/notification",
			.cbs = {
				.modify  = bgp_global_graceful_restart_notification_modify,
				.destroy = bgp_global_graceful_restart_notification_destroy,
				.cli_show = bgp_global_graceful_restart_notification_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/long-lived-stale-time",
			.cbs = {
				.modify  = bgp_global_long_lived_graceful_restart_stale_time_modify,
				.destroy = bgp_global_long_lived_graceful_restart_stale_time_destroy,
				.cli_show = bgp_global_long_lived_graceful_restart_stale_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/fast-convergence",
			.cbs = {
				.modify   = bgp_global_fast_convergence_modify,
				.cli_show = bgp_global_fast_convergence_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/default-link-local-capability",
			.cbs = {
				.modify  = bgp_global_default_link_local_capability_modify,
				.cli_show = bgp_global_default_link_local_capability_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/default-dynamic-capability",
			.cbs = {
				.modify  = bgp_global_default_dynamic_capability_modify,
				.cli_show = bgp_global_default_dynamic_capability_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-reflector/route-reflector-cluster-id",
			.cbs = {
				.modify  = bgp_global_route_reflector_cluster_id_modify,
				.destroy = bgp_global_route_reflector_cluster_id_destroy,
				.cli_show = bgp_global_route_reflector_cluster_id_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/route-reflector/no-client-reflect",
			.cbs = {
				.modify  = bgp_global_no_client_reflect_modify,
				.cli_show = bgp_global_no_client_reflect_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/local-pref",
			.cbs = {
				.modify  = bgp_global_local_pref_modify,
				.cli_show = bgp_global_local_pref_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/ebgp-multihop-connected-route-check",
			.cbs = {
				.modify  = bgp_global_ebgp_multihop_connected_route_check_modify,
				.cli_show = bgp_global_ebgp_multihop_connected_route_check_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/rib-stale-time",
			.cbs = {
				.modify  = bgp_global_graceful_restart_rib_stale_time_modify,
				.cli_show = bgp_global_rib_stale_time_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/preserve-fw-entry",
			.cbs = {
				.modify  = bgp_global_graceful_restart_preserve_fw_entry_modify,
				.cli_show = bgp_global_preserve_fw_entry_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/global/graceful-restart/stale-routes-time",
			.cbs = {
				.modify  = bgp_global_graceful_restart_stale_routes_time_modify,
				.cli_show = bgp_global_stale_routes_time_cli_show,
			},
		},

		/* neighbor */
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor",
			.cbs = {
				.create  = bgp_neighbor_create,
				.destroy = bgp_neighbor_destroy,
				.get_next     = bgp_nb_stub_get_next,
				.get_keys     = bgp_nb_stub_get_keys,
				.lookup_entry = bgp_nb_stub_lookup_entry,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/neighbor-remote-as/remote-as-type",
			.cbs = {
				.modify = bgp_neighbor_remote_as_type_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/neighbor-remote-as/remote-as",
			.cbs = {
				.modify  = bgp_neighbor_remote_as_modify,
				.destroy = bgp_neighbor_remote_as_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/password",
			.cbs = {
				.modify  = bgp_neighbor_password_modify,
				.destroy = bgp_neighbor_password_destroy,
				.cli_show = bgp_neighbor_password_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/description",
			.cbs = {
				.modify  = bgp_neighbor_description_modify,
				.destroy = bgp_neighbor_description_destroy,
				.cli_show = bgp_neighbor_description_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/passive-mode",
			.cbs = {
				.modify  = bgp_neighbor_passive_mode_modify,
				.cli_show = bgp_neighbor_passive_mode_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/solo",
			.cbs = {
				.modify  = bgp_neighbor_solo_modify,
				.cli_show = bgp_neighbor_solo_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/enforce-first-as",
			.cbs = {
				.modify  = bgp_neighbor_enforce_first_as_modify,
				.cli_show = bgp_neighbor_enforce_first_as_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/ttl-security",
			.cbs = {
				.modify  = bgp_neighbor_ttl_security_modify,
				.destroy = bgp_neighbor_ttl_security_destroy,
				.cli_show = bgp_neighbor_ttl_security_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/admin-shutdown/enable",
			.cbs = {
				.modify  = bgp_neighbor_admin_shutdown_enable_modify,
				.destroy = bgp_neighbor_admin_shutdown_enable_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/admin-shutdown/message",
			.cbs = {
				.modify  = bgp_neighbor_admin_shutdown_message_modify,
				.destroy = bgp_neighbor_admin_shutdown_message_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/ebgp-multihop/enabled",
			.cbs = {
				.modify  = bgp_neighbor_ebgp_multihop_enabled_modify,
				.destroy = bgp_neighbor_ebgp_multihop_enabled_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/ebgp-multihop/multihop-ttl",
			.cbs = {
				.modify  = bgp_neighbor_ebgp_multihop_ttl_modify,
				.destroy = bgp_neighbor_ebgp_multihop_ttl_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/ebgp-multihop/disable-connected-check",
			.cbs = {
				.modify  = bgp_neighbor_ebgp_multihop_disable_connected_check_modify,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/update-source/ip",
			.cbs = {
				.modify  = bgp_neighbor_update_source_ip_modify,
				.destroy = bgp_neighbor_update_source_ip_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/update-source/interface",
			.cbs = {
				.modify  = bgp_neighbor_update_source_interface_modify,
				.destroy = bgp_neighbor_update_source_interface_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/timers/connect-time",
			.cbs = {
				.modify  = bgp_neighbor_timers_connect_time_modify,
				.destroy = bgp_neighbor_timers_connect_time_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/timers/advertise-interval",
			.cbs = {
				.modify  = bgp_neighbor_timers_advertise_interval_modify,
				.destroy = bgp_neighbor_timers_advertise_interval_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/local-as",
			.cbs = {
				.apply_finish = bgp_neighbor_local_as_apply_finish,
				.cli_show = bgp_neighbor_local_as_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/bfd-options",
			.cbs = {
				.apply_finish = bgp_neighbor_bfd_options_apply_finish,
				.cli_show = bgp_neighbor_bfd_options_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/dynamic-capability",
			.cbs = {
				.modify  = bgp_neighbor_capabilities_dynamic_modify,
				.cli_show = bgp_neighbor_capabilities_dynamic_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/strict-capability",
			.cbs = {
				.modify  = bgp_neighbor_capabilities_strict_modify,
				.cli_show = bgp_neighbor_capabilities_strict_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/override-capability",
			.cbs = {
				.modify  = bgp_neighbor_capabilities_override_modify,
				.cli_show = bgp_neighbor_capabilities_override_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/extended-nexthop-capability",
			.cbs = {
				.modify  = bgp_neighbor_capabilities_extended_nexthop_modify,
				.cli_show = bgp_neighbor_capabilities_extended_nexthop_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/capability-negotiate",
			.cbs = {
				.modify  = bgp_neighbor_capabilities_negotiate_modify,
				.cli_show = bgp_neighbor_capabilities_negotiate_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/aigp",
			.cbs = {
				.modify  = bgp_neighbor_aigp_modify,
				.cli_show = bgp_neighbor_aigp_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/ip-transparent",
			.cbs = {
				.modify  = bgp_neighbor_ip_transparent_modify,
				.cli_show = bgp_neighbor_ip_transparent_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/extended-link-bandwidth",
			.cbs = {
				.modify  = bgp_neighbor_extended_link_bandwidth_modify,
				.cli_show = bgp_neighbor_extended_link_bandwidth_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/disable-link-bw-encoding-ieee",
			.cbs = {
				.modify  = bgp_neighbor_disable_link_bw_encoding_ieee_modify,
				.cli_show = bgp_neighbor_disable_link_bw_encoding_ieee_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/extended-optional-parameters",
			.cbs = {
				.modify  = bgp_neighbor_extended_optional_parameters_modify,
				.cli_show = bgp_neighbor_extended_optional_parameters_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/send-nexthop-characteristics",
			.cbs = {
				.modify  = bgp_neighbor_send_nexthop_characteristics_modify,
				.cli_show = bgp_neighbor_send_nexthop_characteristics_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/rpki-strict",
			.cbs = {
				.modify  = bgp_neighbor_rpki_strict_modify,
				.cli_show = bgp_neighbor_rpki_strict_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/fqdn-capability",
			.cbs = {
				.modify  = bgp_neighbor_capability_fqdn_modify,
				.cli_show = bgp_neighbor_capability_fqdn_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/link-local-capability",
			.cbs = {
				.modify  = bgp_neighbor_capability_link_local_modify,
				.cli_show = bgp_neighbor_capability_link_local_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/sender-as-path-loop-detection",
			.cbs = {
				.modify  = bgp_neighbor_as_loop_detection_modify,
				.cli_show = bgp_neighbor_as_loop_detection_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/capability-options/software-version-capability",
			.cbs = {
				.modify  = bgp_neighbor_capability_software_version_modify,
				.destroy = bgp_neighbor_capability_software_version_destroy,
				.cli_show = bgp_neighbor_capability_software_version_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/tcp-mss",
			.cbs = {
				.modify  = bgp_neighbor_tcp_mss_modify,
				.destroy = bgp_neighbor_tcp_mss_destroy,
				.cli_show = bgp_neighbor_tcp_mss_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/local-role",
			.cbs = {
				.apply_finish = bgp_neighbor_local_role_apply_finish,
				.cli_show = bgp_neighbor_local_role_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/graceful-restart/enable",
			.cbs = {
				.modify  = bgp_neighbor_gr_enable_modify,
				.destroy = bgp_neighbor_gr_enable_destroy,
				.cli_show = bgp_neighbor_gr_enable_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/graceful-restart/graceful-restart-helper",
			.cbs = {
				.modify  = bgp_neighbor_gr_helper_modify,
				.destroy = bgp_neighbor_gr_helper_destroy,
				.cli_show = bgp_neighbor_gr_helper_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/graceful-restart/graceful-restart-disable",
			.cbs = {
				.modify  = bgp_neighbor_gr_disable_modify,
				.destroy = bgp_neighbor_gr_disable_destroy,
				.cli_show = bgp_neighbor_gr_disable_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/timers",
			.cbs = {
				.apply_finish = bgp_neighbor_timers_apply_finish,
				.cli_show = bgp_neighbor_timers_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/oad",
			.cbs = {
				.modify  = bgp_neighbor_oad_modify,
				.cli_show = bgp_neighbor_oad_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/local-link-id",
			.cbs = {
				.modify  = bgp_neighbor_ls_local_link_id_modify,
				.destroy = bgp_neighbor_ls_local_link_id_destroy,
				.cli_show = bgp_neighbor_ls_local_link_id_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/remote-link-id",
			.cbs = {
				.modify  = bgp_neighbor_ls_remote_link_id_modify,
				.destroy = bgp_neighbor_ls_remote_link_id_destroy,
				.cli_show = bgp_neighbor_ls_remote_link_id_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/graceful-shutdown",
			.cbs = {
				.modify  = bgp_neighbor_peer_graceful_shutdown_modify,
				.cli_show = bgp_neighbor_peer_graceful_shutdown_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/timers/delayopen",
			.cbs = {
				.modify  = bgp_neighbor_timers_delayopen_modify,
				.destroy = bgp_neighbor_timers_delayopen_destroy,
				.cli_show = bgp_neighbor_timers_delayopen_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/admin-shutdown",
			.cbs = {
				.apply_finish = bgp_neighbor_admin_shutdown_apply_finish,
				.cli_show = bgp_neighbor_admin_shutdown_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/neighbors/neighbor/admin-shutdown/rtt",
			.cbs = {
				.modify   = bgp_nb_stub_modify,
				.destroy  = bgp_neighbor_admin_shutdown_rtt_destroy,
				.cli_show = bgp_nb_handled_by_parent_cli_show,
			},
		},

		/* per-AF per-peer flag toggles */
#define BGP_NB_AF_XPATH(_leaf)                                                 \
	"/frr-routing:routing/control-plane-protocols/control-plane-protocol/" \
	"frr-bgp:bgp/neighbors/neighbor/afi-safis/afi-safi/" _leaf
#define BGP_NB_AF_CONT_XPATH(_cont, _leaf)                                     \
	"/frr-routing:routing/control-plane-protocols/control-plane-protocol/" \
	"frr-bgp:bgp/neighbors/neighbor/afi-safis/afi-safi/" _cont "/" _leaf
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "soft-reconfiguration"),
		  .cbs = {
			  .modify = bgp_neighbor_af_soft_reconfig_in_modify,
			  .cli_show = bgp_neighbor_af_soft_reconfig_in_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unreachability",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unreachability",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "as-path-options/replace-peer-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_as_override_modify,
			  .cli_show = bgp_neighbor_af_as_override_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "route-reflector/route-reflector-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rr_client_modify,
			  .cli_show = bgp_neighbor_af_rr_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "route-server/route-server-client"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rs_client_modify,
			  .cli_show = bgp_neighbor_af_rs_client_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "nexthop-self/next-hop-self"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "nexthop-self/next-hop-self-force"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_self_force_modify,
			  .cli_show = bgp_neighbor_af_nexthop_self_force_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "private-as/remove-private-as"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "private-as/remove-private-as-all"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "private-as/remove-private-as-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "private-as/remove-private-as-all-replace"),
		  .cbs = {
			  .modify = bgp_neighbor_af_remove_private_as_all_replace_modify,
			  .cli_show = bgp_neighbor_af_remove_private_as_all_replace_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "nexthop-local-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_nexthop_local_unchanged_modify,
			  .cli_show = bgp_neighbor_af_nexthop_local_unchanged_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "send-community/send-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_community_modify,
			  .cli_show = bgp_neighbor_af_send_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "send-community/send-ext-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_ext_community_modify,
			  .cli_show = bgp_neighbor_af_send_ext_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "send-community/send-large-community"),
		  .cbs = {
			  .modify = bgp_neighbor_af_send_large_community_modify,
			  .cli_show = bgp_neighbor_af_send_large_community_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "accept-own"),
		  .cbs = {
			  .modify = bgp_neighbor_af_accept_own_modify,
			  .cli_show = bgp_neighbor_af_accept_own_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "add-paths/disable-addpath-rx"),
		  .cbs = {
			  .modify = bgp_neighbor_af_disable_addpath_rx_modify,
			  .cli_show = bgp_neighbor_af_disable_addpath_rx_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "add-paths/path-type"),
		  .cbs = {
			  .modify = bgp_neighbor_af_add_paths_path_type_modify,
			  .cli_show = bgp_neighbor_af_add_paths_path_type_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_XPATH("enabled"),
		  .cbs = { .modify = bgp_neighbor_af_enabled_modify,
			   .destroy = bgp_neighbor_af_enabled_destroy,
			   .cli_show = bgp_nb_handled_by_parent_cli_show } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "encapsulation/type"),
		  .cbs = {
			  .create = bgp_neighbor_af_encapsulation_type_create,
			  .destroy = bgp_neighbor_af_encapsulation_type_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "encapsulation/type"),
		  .cbs = {
			  .create = bgp_neighbor_af_encapsulation_type_create,
			  .destroy = bgp_neighbor_af_encapsulation_type_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "encapsulation/type"),
		  .cbs = {
			  .create = bgp_neighbor_af_encapsulation_type_create,
			  .destroy = bgp_neighbor_af_encapsulation_type_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "encapsulation/type"),
		  .cbs = {
			  .create = bgp_neighbor_af_encapsulation_type_create,
			  .destroy = bgp_neighbor_af_encapsulation_type_destroy,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "attr-unchanged/as-path-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_as_path_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "attr-unchanged/next-hop-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_next_hop_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "attr-unchanged/med-unchanged"),
		  .cbs = {
			  .modify = bgp_neighbor_af_attr_unchanged_med_modify,
			  .cli_show = bgp_nb_handled_by_parent_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "filter-config/rmap-import"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_import_modify,
			  .destroy = bgp_neighbor_af_rmap_import_destroy,
			  .cli_show = bgp_neighbor_af_rmap_import_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-flowspec",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-labeled-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-multicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv4-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-flowspec",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-labeled-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-multicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("ipv6-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l2vpn-evpn",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv4-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
		{ .xpath = BGP_NB_AF_CONT_XPATH("l3vpn-ipv6-unicast",
						     "filter-config/rmap-export"),
		  .cbs = {
			  .modify = bgp_neighbor_af_rmap_export_modify,
			  .destroy = bgp_neighbor_af_rmap_export_destroy,
			  .cli_show = bgp_neighbor_af_rmap_export_cli_show,
		  } },
#undef BGP_NB_AF_XPATH

		/* peer-group */
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/peer-groups/peer-group",
			.cbs = {
				.create  = bgp_peer_group_create,
				.destroy = bgp_peer_group_destroy,
				.get_next     = bgp_nb_stub_get_next,
				.get_keys     = bgp_nb_stub_get_keys,
				.lookup_entry = bgp_nb_stub_lookup_entry,
				.cli_show = bgp_peer_group_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/peer-groups/peer-group/ipv4-listen-range",
			.cbs = {
				.create  = bgp_peer_group_ipv4_listen_range_create,
				.destroy = bgp_peer_group_ipv4_listen_range_destroy,
				.cli_show = bgp_peer_group_ipv4_listen_range_cli_show,
			},
		},
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol/frr-bgp:bgp/peer-groups/peer-group/ipv6-listen-range",
			.cbs = {
				.create  = bgp_peer_group_ipv6_listen_range_create,
				.destroy = bgp_peer_group_ipv6_listen_range_destroy,
				.cli_show = bgp_peer_group_ipv6_listen_range_cli_show,
			},
		},

		{
			.xpath = NULL,
		},
	},
};
/* clang-format on */
