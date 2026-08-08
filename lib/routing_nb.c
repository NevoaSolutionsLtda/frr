// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018        Vmware
 *                           Vishal Dhingra
 */
#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"
#include "routing_nb.h"

static const void *cpp_get_next(struct nb_cb_get_next_args *args)
{
	return NULL;
}

static int cpp_get_keys(struct nb_cb_get_keys_args *args)
{
	args->keys->num = 0;
	return NB_OK;
}

static const void *cpp_lookup_entry(struct nb_cb_lookup_entry_args *args)
{
	return NULL;
}

/* clang-format off */
const struct frr_yang_module_info frr_routing_info = {
	.name = "frr-routing",
	.nodes = {
		{
			.xpath = "/frr-routing:routing/control-plane-protocols/control-plane-protocol",
			.cbs = {
				.create = routing_control_plane_protocols_control_plane_protocol_create,
				.destroy = routing_control_plane_protocols_control_plane_protocol_destroy,
				/*
				 * Deleting the list entry must also run the
				 * destroy callbacks of the per-protocol child
				 * containers (e.g. frr-bgp:bgp), otherwise the
				 * daemon keeps the live instance while the
				 * config datastore loses it.
				 */
				.flags = F_NB_CB_DESTROY_RECURSE,
				.get_next = cpp_get_next,
				.get_keys = cpp_get_keys,
				.lookup_entry = cpp_lookup_entry,
			}
		},
		{
			.xpath = NULL,
		},
	}
};

const struct frr_yang_module_info frr_routing_cli_info = {
	.name = "frr-routing",
	.ignore_cfg_cbs = true,
	.nodes = {
		{
			.xpath = NULL,
		},
	}
};
