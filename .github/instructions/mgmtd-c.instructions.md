---
applyTo: "mgmtd/**/*.c"
---

# mgmtd/*.c discipline

## P1 — do NOT silently change YANG list key definitions

Flag any change in `mgmtd/**/*.c` that alters the set of keys on a
YANG list — whether in schema `*.yang` files touched in the PR, in
xpath builders, or in runtime validators.

Silently changing keys is YANG NBC (non-backward-compatible): it
breaks anyone configuring FRR through direct YANG (for example,
Netconf clients, RESTCONF UIs, or any third-party controller that
persisted state against the old schema).

When a key change is intentional and justified:

1. The commit message must call out the NBC break explicitly.
2. A new schema version must be bumped and reflected in release
   notes.
3. A migration path (or explicit lack thereof) must be documented.

Reference: PR #21296 choppsv1 NACK.

## P2 — do NOT duplicate output functions between JSON and non-JSON paths

Flag any new function, or any extension of existing code, that
creates a separate code path for JSON output vs plain-text output of
the same information.

Single output path avoids drift: what a human sees via vtysh and what
a controller sees via JSON should always agree. Duplicated paths
diverge over time; fields appear in one but not the other; bug fixes
hit one but not the other.

Preferred shape: a single output function that takes a vty-or-json
sink parameter, or uses FRR's existing `json_object` helpers so the
same traversal fills both.

Reference: pattern referenced by choppsv1 in #21232.

### Exceptions

- Legacy output that has been JSON-only or vty-only for a long time
  is not a reason to block the PR touching it. But a *new* addition
  that creates a new plaintext-only or json-only output is in scope.

## P2 — event-loop discipline

FRR daemons run on a single libevent / thread loop. The only
preemption points are explicit yields (`event_add_*`, `event_cancel`,
blocking reads that are actually `THREAD_READ`, etc.).

Flag any new long-running loop or blocking syscall in
`mgmtd/**/*.c` that could stall the loop for more than a few
milliseconds. That includes:

- Unbounded `while` iteration over unbounded external input without
  an event yield.
- `sleep()` or `usleep()` on any non-test path.
- File I/O on an uncached filesystem without an async abstraction.

## Scope of this file

Path glob: `mgmtd/**/*.c`. Concurrency-sensitive files under
`mgmtd/mgmt_txn*.[ch]` have an additional, stricter checklist in
`mgmtd-txn.instructions.md`. Both apply in aggregate.
