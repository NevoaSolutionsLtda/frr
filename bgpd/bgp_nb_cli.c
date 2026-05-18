// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal frr-bgp YANG module registration for mgmtd. Compiled into
 * mgmtd's libmgmt_be_nb so mgmtd recognises the frr-bgp xpath subtree
 * and dispatches writes to the bgpd backend client.
 *
 * The real callback table (with create/modify/destroy) lives in bgpd's
 * bgp_nb.c (`frr_bgp_info`). The stub here uses `ignore_cfg_cbs=true`
 * so mgmtd will not validate callback presence.
 *
 * Copyright (C) 2026 FRRouting
 */

#include <zebra.h>

#include "lib/northbound.h"

const struct frr_yang_module_info frr_bgp_cli_info = {
	.name = "frr-bgp",
	.ignore_cfg_cbs = true,
	.nodes = {
		{
			.xpath = NULL,
		},
	},
};
