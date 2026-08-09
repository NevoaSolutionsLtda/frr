// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Northbound callbacks used to satisfy nb_validate_callbacks() for
 * YANG nodes in the frr-bgp tree that have not yet been wired to real
 * bgpd implementations. Two classes exist (see the class column in
 * bgp_nb_stubs_table.inc, assigned by tools/gen-bgp-nb-stubs.py):
 *
 * - warn (default): NB_OK no-op. Programmatic (non-CLI) writes are
 *   counted and reported through an aggregated log warning so that
 *   "commit OK without effect" is visible without flooding the log.
 *
 * - reject-strict (MGC core: EVPN, prefix-limit, redistribution-list,
 *   network-config): programmatic writes fail commit validation with
 *   an explicit error, so a management client never receives a false
 *   commit-OK on those subtrees.
 *
 * Writes originating from the CLI dual-write path (NB_CLIENT_CLI) are
 * exempt from both the warning and the rejection: the legacy DEFUN has
 * already applied the change to the daemon, and the NB write only
 * mirrors it into the YANG datastore. Rejecting or warning there would
 * break commands that remain under legacy-CLI authority.
 *
 * When you wire a real callback for an xpath, remove its line from
 * tools/missing_cbs.tsv and regenerate bgp_nb_stubs_table.inc.
 */
#include <zebra.h>

#include "lib/log.h"
#include "lib/monotime.h"
#include "lib/northbound.h"
#include "lib/yang.h"

#include "bgpd/bgp_nb_stubs.h"

static bool bgp_nb_stub_client_is_cli(const struct nb_context *context)
{
	return context && context->client == NB_CLIENT_CLI;
}

/*
 * Aggregated warning for programmatic writes that land on warn-class
 * stubs: at most one log line per second, carrying the number of stub
 * hits since the previous line and the latest xpath. Hits after the
 * last line of a burst stay counted and are flushed by the next burst.
 */
static void bgp_nb_stub_warn_hit(const struct lyd_node *dnode)
{
	static time_t last_log_sec;
	static unsigned int hits;
	char xpath[XPATH_MAXLEN];
	time_t now = monotime(NULL);

	hits++;
	if (now == last_log_sec)
		return;

	yang_dnode_get_path(dnode, xpath, sizeof(xpath));
	zlog_warn("bgpd northbound: %u write(s) to unimplemented node(s) accepted as no-op since last warning (latest: %s)",
		  hits, xpath);
	last_log_sec = now;
	hits = 0;
}

static int bgp_nb_stub_reject(const struct nb_context *context,
			      enum nb_event event,
			      const struct lyd_node *dnode, char *errmsg,
			      size_t errmsg_len)
{
	char xpath[XPATH_MAXLEN];

	if (bgp_nb_stub_client_is_cli(context))
		return NB_OK;
	if (event != NB_EV_VALIDATE)
		return NB_OK;

	yang_dnode_get_path(dnode, xpath, sizeof(xpath));
	snprintfrr(errmsg, errmsg_len,
		   "unimplemented in bgpd northbound (reject-strict class): %s",
		   xpath);
	return NB_ERR_VALIDATION;
}

int bgp_nb_stub_create(struct nb_cb_create_args *args)
{
	if (args->event == NB_EV_APPLY &&
	    !bgp_nb_stub_client_is_cli(args->context))
		bgp_nb_stub_warn_hit(args->dnode);
	return NB_OK;
}

int bgp_nb_stub_modify(struct nb_cb_modify_args *args)
{
	if (args->event == NB_EV_APPLY &&
	    !bgp_nb_stub_client_is_cli(args->context))
		bgp_nb_stub_warn_hit(args->dnode);
	return NB_OK;
}

int bgp_nb_stub_destroy(struct nb_cb_destroy_args *args)
{
	if (args->event == NB_EV_APPLY &&
	    !bgp_nb_stub_client_is_cli(args->context))
		bgp_nb_stub_warn_hit(args->dnode);
	return NB_OK;
}

int bgp_nb_stub_reject_create(struct nb_cb_create_args *args)
{
	return bgp_nb_stub_reject(args->context, args->event, args->dnode,
				  args->errmsg, args->errmsg_len);
}

int bgp_nb_stub_reject_modify(struct nb_cb_modify_args *args)
{
	return bgp_nb_stub_reject(args->context, args->event, args->dnode,
				  args->errmsg, args->errmsg_len);
}

int bgp_nb_stub_reject_destroy(struct nb_cb_destroy_args *args)
{
	return bgp_nb_stub_reject(args->context, args->event, args->dnode,
				  args->errmsg, args->errmsg_len);
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
