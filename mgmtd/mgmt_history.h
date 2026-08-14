// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021  Vmware, Inc.
 *		       Pushpasis Sarkar <spushpasis@vmware.com>
 * Copyright (c) 2023, LabN Consulting, L.L.C.
 *
 */
#ifndef _FRR_MGMTD_HISTORY_H_
#define _FRR_MGMTD_HISTORY_H_

#include "vrf.h"

PREDECL_DLIST(mgmt_cmt_infos);

struct mgmt_ds_ctx;

/*
 * Rollback specific commit from commit history.
 *
 * vty
 *    VTY context.
 *
 * cmtid_str
 *    Specific commit id from commit history.
 *
 * Returns:
 *    0 on success, -1 on failure.
 */
extern int mgmt_history_rollback_by_id(struct vty *vty, const char *cmtid_str);

/*
 * Rollback n commits from commit history.
 *
 * vty
 *    VTY context.
 *
 * num_cmts
 *    Number of commits to be rolled back.
 *
 * Returns:
 *    0 on success, -1 on failure.
 */
extern int mgmt_history_rollback_n(struct vty *vty, int num_cmts);

extern void mgmt_history_rollback_complete(bool success);

/*
 * Show mgmt commit history.
 */
extern void show_mgmt_cmt_history(struct vty *vty);

extern void mgmt_history_new_record(struct mgmt_ds_ctx *ds_ctx);

extern void mgmt_history_destroy(void);
extern void mgmt_history_init(void);

struct nb_config;

/*
 * gRPC commit-history readers (issue #29): expose the last
 * MGMTD_MAX_COMMIT_LIST commits to the northbound transaction RPCs.
 * Both run on the caller's thread, like the rollback listing.
 */
extern int mgmt_history_transactions_iterate(
	void (*func)(void *arg, int transaction_id, const char *client_name,
		     const char *date, const char *comment),
	void *arg);
extern struct nb_config *mgmt_history_transaction_load(uint32_t transaction_id);

/*
 * Transaction id of the most recent commit record, or 0 when the
 * history is empty.  The id is the FNV-1a hash of the commit-id string,
 * so it matches what the transaction RPCs report.  mgmtd runs a single
 * event loop, so a caller that has just created a record (commit
 * completion) owns the head of the list.
 */
extern uint32_t mgmt_history_last_cmt_txn_id(void);

/*
 * 012345678901234567890123456789
 * 2023-12-31T12:12:12,012345678
 * 20231231121212012345678
 */
#define MGMT_LONG_TIME_FMT "%Y-%m-%dT%H:%M:%S"
#define MGMT_LONG_TIME_MAX_LEN 30
#define MGMT_SHORT_TIME_FMT "%Y%m%d%H%M%S"
#define MGMT_SHORT_TIME_MAX_LEN 24

static inline const char *
mgmt_time_to_string(struct timespec *tv, bool long_fmt, char *buffer, size_t sz)
{
	struct tm tm;
	size_t n;

	localtime_r(&tv->tv_sec, &tm);

	if (long_fmt) {
		n = strftime(buffer, sz, MGMT_LONG_TIME_FMT, &tm);
		assert(n < sz);
		snprintf(&buffer[n], sz - n, ",%09lu", tv->tv_nsec);
	} else {
		n = strftime(buffer, sz, MGMT_SHORT_TIME_FMT, &tm);
		assert(n < sz);
		snprintf(&buffer[n], sz - n, "%09lu", tv->tv_nsec);
	}

	return buffer;
}

static inline const char *mgmt_realtime_to_string(struct timeval *tv, char *buf,
						  size_t sz)
{
	struct timespec ts = {.tv_sec = tv->tv_sec,
			      .tv_nsec = tv->tv_usec * 1000};

	return mgmt_time_to_string(&ts, true, buf, sz);
}

#endif /* _FRR_MGMTD_HISTORY_H_ */
