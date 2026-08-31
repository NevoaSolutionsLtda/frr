// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018        Vmware
 *                           Vishal Dhingra
 */
#include <zebra.h>

#include "northbound.h"
#include "libfrr.h"
#include "routing_nb.h"

/* clang-format off */
DEFINE_HOOK(routing_control_plane_protocol_oper_next,
	    (const void **entry), (entry));
DEFINE_HOOK(routing_control_plane_protocol_oper_keys,
	    (const void *entry, struct yang_list_keys *keys), (entry, keys));
DEFINE_HOOK(routing_control_plane_protocol_oper_lookup,
	    (const struct yang_list_keys *keys, const void **entry),
	    (keys, entry));
/* clang-format on */

static const void *cpp_get_next(struct nb_cb_get_next_args *args)
{
	const void *entry = args->list_entry;

	if (hook_call(routing_control_plane_protocol_oper_next, &entry))
		return entry;

	return NULL;
}

static int cpp_get_keys(struct nb_cb_get_keys_args *args)
{
	if (hook_call(routing_control_plane_protocol_oper_keys,
		      args->list_entry, args->keys))
		return NB_OK;

	args->keys->num = 0;
	return NB_OK;
}

static const void *cpp_lookup_entry(struct nb_cb_lookup_entry_args *args)
{
	const void *entry = NULL;

	hook_call(routing_control_plane_protocol_oper_lookup, args->keys,
		  &entry);
	return entry;
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
				 * config datastore loses it. Children run
				 * first; the per-daemon routing_destroy hook
				 * (e.g. staticd) still runs exactly once,
				 * afterwards, on a table the child callbacks
				 * already emptied (verified live: staticd
				 * survives an entry delete with its routes
				 * cleanly withdrawn).
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
