---
applyTo: "mgmtd/mgmt_txn*.[ch]"
---

# mgmtd/mgmt_txn*.[ch] — concurrency review checklist

These files carry CONFIG-txn locking, admission gating, and
finishing-slot lifecycle logic. Patches here almost always change
shared-state semantics even when the diff is small. The cost of
missing a subtle overlap bug is high: fleets hit it under load days
or weeks later.

**Before returning "No actionable comments", a Copilot review of a
patch in this scope MUST explicitly address each of the five points
below.** Treat the absence of any one as a P2 finding ("this review
did not consider overlap / refcount / …").

## (1) Overlap window

Name every mgmtd global or singleton that the old txn and the new
txn can touch concurrently after any lock or gate is released early.
Cite file:line for each shared symbol.

Patches changing any of these are in-scope:

- `config_active_txn`
- `txn_config_txn`
- `mgmt_create_txn`
- `txn_req_free`
- `mgmt_ds_*_lock`

Reference: UB-14 narrowing, empirical lab regression 2026-04-22.

## (2) Refcount integrity under delayed teardown

If a global is cleared before the txn's refcount drops, verify no
late backend reply or subscriber callback dereferences the old txn
via that global.

Look for: backend send-in-flight at the time of early clear;
subscribers registered on the old txn; timers or events queued with
a pointer-to-txn payload.

## (3) Error cascade

Between any early unlock and the FE reply, ask: if
`mgmt_fe_send_*_reply` fails or the session is gone (returns
`-ENOENT`), is cleanup of the pre-cleared DS locks still reached?

Before/after behaviour must match on the error path. Many patches
move the success path and forget the error-cleanup path.

## (4) Event-loop yield points

FRR daemons run on a single libevent loop. The only preemption is
at explicit yield points (`event_add_read`, `event_add_timer`, etc.).
Identify which (if any) of the mutation steps yield, and whether
another txn's admission path can observe a half-completed state at
that yield.

Narrowing a critical section is safe only if every yield between
"lock drop" and "state consistent" is proven unobservable.

## (5) Test coverage

For any patch that:

- narrows or widens a critical section,
- releases a global singleton earlier than before,
- changes the invariant governing concurrent execution,

flag absence of a regression test that exercises the new invariant
directly. Aggregate throughput / latency benches do NOT count.

An overlap-or-serialisation-observable test, for example an
**asymmetric-payload wall-clock** pattern — see
`tests/topotests/mgmt_config/test_pipelined_commits.py` — does count.
The asymmetric-payload shape is: two commits A (large payload) and
B (small payload) issued simultaneously; if pipelining holds, B
finishes before A; a future revert of the patch makes this
assertion fail.

## Label

A review on files under this glob that gives no P1/P2 findings on
any of the five points above should include the sentence: "Reviewed
overlap, refcount, cascade, yield, and test — no findings." Absence
of that sentence is a P2 on the review itself.

## Scope of this file

Path glob: `mgmtd/mgmt_txn*.[ch]`. This is the strictest scope in
the repo. Both this file and `mgmtd-c.instructions.md` apply.
