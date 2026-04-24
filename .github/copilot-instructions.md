# Copilot code review instructions — reinaldosaraiva/frr staging-review

This file governs Copilot code review on pull requests targeting the
`staging-review` branch of this fork. It replaces the former
`.coderabbit.yaml` (removed in commit `547112d250`) and encodes review
discipline learned from NACKs on the FRRouting/frr upstream.

Reference PRs that informed these rules: #21538 (wontfix), #21557
(wontfix), #21514 (amended), #21051 (NACK over-engineered), #21459
(NACK too many changes), #21296 (NACK NBC list key), #19480
(whitespace rejection).

## Tone

Write as an engineer at a keyboard reviewing a colleague's patch.
Direct, technical, concise. No emoji. Cite PR numbers or file:line
when flagging a known pattern. Keep each inline comment shorter than
the diff it references.

Label findings by severity:

- **P1** — correctness-critical. Would break production or hide a
  real bug. Must block merge.
- **P2** — correctness concern or maintainability red flag. Worth
  fixing but not a blocker by itself.
- **P3** — nit. Style, wording, micro-optimisation. Do not block.

Call out absence of a required change too. If the patch claims to fix
behaviour X and there is no regression test for X, that is a P2.

## Banned libc functions

Flag any new call to `sprintf`, `strcat`, `strcpy`, `inet_ntoa`, or
`ctime`. FRR requires `snprintf`, `strlcat`, `strlcpy`, `inet_ntop`,
and `ctime_r` respectively. Reference: PR #18436 Jafaral inline.

## Whitespace-only / cosmetic changes mixed into functional PR

Flag any whitespace-only, indentation-only, or comment-cleanup chunk
that is included in a PR that otherwise changes behaviour. Cosmetic
fixes belong in a separate commit or a separate PR. Reference:
PRs #19480, #20031, #18436 — Jafaral repeatedly requested drops of
such mixed diffs.

## Demand-test for behavioural invariants

If the commit message claims a new behavioural invariant — examples:
pipelining, early unlock, reordered state transitions, narrowed
critical section, relaxed ordering, new admission gate, deferred
teardown — flag **absence of a regression test that exercises the
invariant itself**.

A positive-invariant assertion (for example "B finishes before A when
payloads are asymmetric") is the standard. Reconciler throughput
numbers or aggregate latency are NOT sufficient evidence. Coexistence
in running-config alone is not a test. The test must be structured so
a future revert of the patch makes it fail.

Reference: `tests/topotests/mgmt_config/test_pipelined_commits.py`
(2026-04 UB-14 upstream topotest) is the canonical shape.

## Overclaiming in commit messages

Reject commits that call themselves "perfect", "flawless",
"guaranteed", "zero bugs", "100% correct", or similar superlatives.
Claims must match what the patch actually demonstrates.

## Path-scoped rules

Additional rules are under `.github/instructions/*.instructions.md`
with `applyTo:` globs targeting `lib/**/*.h`, `lib/mgmt_*.h`,
`mgmtd/**/*.c`, `mgmtd/mgmt_txn*.[ch]`, and `doc/user/**`. Copilot
loads both this file and any matching path-scoped file for a given
diff; findings from both are aggregated into the review.

## Scope of this file

- This file lives on `staging-review` branch only.
  `upstream-submit/*` branches must stay clean so these instructions
  never leak into a PR sent to `FRRouting/frr`.
- Only the first 4000 characters of this file are read by Copilot.
  Keep edits below that budget.
