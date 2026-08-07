// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * No-op northbound callbacks used to satisfy nb_validate_callbacks()
 * for YANG nodes in the frr-bgp tree that have not yet been wired to
 * real bgpd implementations.
 *
 * A stub at NB_EV_APPLY is intentionally a no-op (returns NB_OK)
 * rather than NB_ERR. Returning NB_ERR would surface as a generic
 * commit-time failure with no context for the operator; the no-op
 * behaviour preserves legacy-CLI authority for unwired knobs while
 * allowing mgmtd commits that touch *only* wired nodes to succeed.
 *
 * When you wire a real callback for an xpath, remove its line from
 * tools/missing_cbs.tsv and regenerate bgp_nb_stubs_table.inc.
 */
#include <zebra.h>

#include "lib/northbound.h"

#include "bgpd/bgp_nb_stubs.h"

int bgp_nb_stub_create(struct nb_cb_create_args *args)
{
	return NB_OK;
}

int bgp_nb_stub_modify(struct nb_cb_modify_args *args)
{
	return NB_OK;
}

int bgp_nb_stub_destroy(struct nb_cb_destroy_args *args)
{
	return NB_OK;
}

const void *bgp_nb_stub_get_next(struct nb_cb_get_next_args *args)
{
	return NULL;
}

int bgp_nb_stub_get_keys(struct nb_cb_get_keys_args *args)
{
	args->keys->num = 0;
	return NB_OK;
}

const void *bgp_nb_stub_lookup_entry(struct nb_cb_lookup_entry_args *args)
{
	return NULL;
}
