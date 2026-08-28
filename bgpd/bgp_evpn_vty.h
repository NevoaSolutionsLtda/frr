// SPDX-License-Identifier: GPL-2.0-or-later
/* EVPN VTY functions to EVPN
 * Copyright (C) 2017 6WIND
 */

#ifndef _FRR_BGP_EVPN_VTY_H
#define _FRR_BGP_EVPN_VTY_H

extern void bgp_config_write_evpn_info(struct vty *vty, struct bgp *bgp,
				       afi_t afi, safi_t safi);
extern void bgp_ethernetvpn_init(void);

#define L2VPN_HELP_STR        "Layer 2 Virtual Private Network\n"
#define EVPN_HELP_STR        "Ethernet Virtual Private Network\n"
#define VNI_HELP_STR "VXLAN Network Identifier\n"
#define VNI_NUM_HELP_STR "VNI number\n"
#define VNI_ALL_HELP_STR "All VNIs\n"
#define DETAIL_HELP_STR "Print Detailed Output\n"
#define VTEP_HELP_STR "Remote VTEP\n"
#define VTEP_IP_HELP_STR "Remote VTEP IPv4 address\n"
#define VTEP_IPV6_HELP_STR "Remote VTEP IPv6 address\n"

extern int argv_find_and_parse_oly_idx(struct cmd_token **argv, int argc,
				       int *oly_idx,
				       enum overlay_index_type *oly);

/* Parse type from "type <ead|1|...>", return -1 on failure */
extern int bgp_evpn_cli_parse_type(int *type, struct cmd_token **argv,
				   int argc);

extern int bgp_evpn_show_all_routes(struct vty *vty, struct bgp *bgp, int type,
				    bool use_json, int detail);

#endif /* _QUAGGA_BGP_EVPN_VTY_H */

/*
 * EVPN configuration internals shared with the BGP northbound
 * callbacks (bgp_nb_config.c). These are the same helpers the CLI
 * DEFUNs call, so both paths produce identical daemon state.
 */
extern void evpn_set_advertise_all_vni(struct bgp *bgp);
extern void evpn_unset_advertise_all_vni(struct bgp *bgp);
extern void evpn_set_advertise_default_gw(struct bgp *bgp,
					  struct bgpevpn *vpn);
extern void evpn_unset_advertise_default_gw(struct bgp *bgp,
					    struct bgpevpn *vpn);
extern void evpn_set_advertise_svi_macip(struct bgp *bgp,
					 struct bgpevpn *vpn, uint32_t set);
extern void evpn_set_advertise_subnet(struct bgp *bgp,
				      struct bgpevpn *vpn);
extern void evpn_unset_advertise_subnet(struct bgp *bgp,
					struct bgpevpn *vpn);
extern void evpn_process_default_originate_cmd(struct bgp *bgp_vrf,
					       afi_t afi, bool add);
extern void evpn_set_autort_rfc8365(struct bgp *bgp, bool import,
				    bool export);
extern void evpn_unset_autort_rfc8365(struct bgp *bgp, bool import,
				      bool export);
extern void bgp_evpn_set_unset_resolve_overlay_index(struct bgp *bgp,
						     bool set);
extern void evpn_configure_vrf_rd(struct bgp *bgp_vrf,
				  struct prefix_rd *rd,
				  const char *rd_pretty);
extern void evpn_unconfigure_vrf_rd(struct bgp *bgp_vrf);
extern void evpn_configure_rd(struct bgp *bgp, struct bgpevpn *vpn,
			      struct prefix_rd *rd, const char *rd_pretty);
extern void evpn_unconfigure_rd(struct bgp *bgp, struct bgpevpn *vpn);
extern struct bgpevpn *evpn_create_update_vni(struct bgp *bgp, vni_t vni);
extern void evpn_delete_vni(struct bgp *bgp, struct bgpevpn *vpn);
extern int vrf_rt_add(struct bgp *bgp, struct bgp_evpn_cfgd_rt *cfgd_rt,
		      enum bgp_evpn_rt_direction rt_direction);
extern int vrf_rt_del(struct bgp *bgp,
		      const struct bgp_evpn_cfgd_rt *cfgd_rt,
		      enum bgp_evpn_rt_direction rt_direction);
extern int l2vni_rt_add(struct bgp *bgp, struct bgpevpn *vpn,
			struct bgp_evpn_cfgd_rt *cfgd_rt,
			enum bgp_evpn_rt_direction rt_direction);
extern int l2vni_rt_del(struct bgp *bgp, struct bgpevpn *vpn,
			const struct bgp_evpn_cfgd_rt *cfgd_rt,
			enum bgp_evpn_rt_direction rt_direction);
extern int bgp_evpn_advertise_type5_set(struct bgp *bgp_vrf, afi_t afi,
					bool gateway_ip,
					const char *rmap_name);
extern void bgp_evpn_advertise_type5_unset(struct bgp *bgp_vrf, afi_t afi);
extern void bgp_evpn_pip_enable_set(struct bgp *bgp_vrf, bool enable);
extern void bgp_evpn_pip_ip_set(struct bgp *bgp_vrf, struct in_addr ip);
extern void bgp_evpn_pip_ip_unset(struct bgp *bgp_vrf);
extern void bgp_evpn_pip_mac_set(struct bgp *bgp_vrf,
				 const struct ethaddr *mac);
extern void bgp_evpn_pip_mac_unset(struct bgp *bgp_vrf);
extern bool bgp_evpn_rt_matches_existing(struct list *rtl,
					 struct ecommunity *ecomtarget);
