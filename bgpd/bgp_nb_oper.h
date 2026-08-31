// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026        Nevoa Solutions Ltda.
 */

#ifndef _FRR_BGP_NB_OPER_H_
#define _FRR_BGP_NB_OPER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* B5.1: bgpd operational state serving. */
extern const struct frr_yang_module_info frr_bgp_oper_info;

/* Real iteration callbacks for the frr-bgp neighbors/neighbor trunk
 * list (registered from bgpd/bgp_nb.c, implemented here).
 */
const void *bgp_nb_oper_neighbor_get_next(struct nb_cb_get_next_args *args);
int bgp_nb_oper_neighbor_get_keys(struct nb_cb_get_keys_args *args);
const void *bgp_nb_oper_neighbor_lookup_entry(
	struct nb_cb_lookup_entry_args *args);

void bgp_nb_oper_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _FRR_BGP_NB_OPER_H_ */
