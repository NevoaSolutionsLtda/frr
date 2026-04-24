---
applyTo: "lib/mgmt_*.h"
---

# lib/mgmt_*.h wire-protocol header discipline

These headers define the mgmtd wire protocol and error taxonomy.
Changes are high-leverage and reviewers are strict about them.

## P1 — new symbolic constant / macro / enum needs an in-tree caller in the same PR

Flag any new `#define`, `enum` member, or preprocessor constant added
to `lib/mgmt_msg_native.h`, `lib/mgmt_fe_client.h`, `lib/mgmt_be_client.h`,
or any `lib/mgmt_*.h` file, that is not referenced by at least one
non-header file in the same PR.

Header-only preparatory PRs need an explicit justification in the
commit body AND a linked follow-up PR that introduces consumers.
Without that, the symbol is speculative vocabulary and the PR should
be held.

Reference: PR #21557 wontfix; Jafaral NACK: "defines errors values
but I don't see those used".

### What counts as a caller

- `#define FOO` is called if `FOO` appears outside `#define` on a
  non-header file in the diff.
- `enum E { E_FOO }` member is called if `E_FOO` appears in a switch,
  assignment, comparison, or function argument in the diff.
- A macro expanded only by the test harness is weak but acceptable
  if the PR is explicitly a test-plumbing PR.

## P1 — do NOT hardcode raw errno integers behind an FRR-owned symbol

Flag any new `MGMT_MSG_ERR_*` or wire-protocol symbolic constant
whose value is a hardcoded `errno` integer (for example `-74` for
`EBADMSG`, `-22` for `EINVAL`).

`errno` numeric values are NOT portable across libcs: Darwin, Linux
glibc, musl, and FreeBSD diverge. Empirical: 4 of the first 11 errno
values differ between Darwin and Linux glibc. Hardcoding Linux values
under FRR-owned names silently miscompiles on other platforms and
corrupts the wire interpretation.

Reference: PR #21557 choppsv1 NACK 2026-04-21. If a stable wire
contract is needed, the correct shape is:

1. Define an FRR-owned enum with its own stable integer values.
2. Translate from `errno` to that enum at the boundary where the
   value enters mgmtd.
3. Only the enum crosses the wire. Never raw `errno`.

### Good

```c
enum mgmt_msg_err {
    MGMT_MSG_ERR_OK = 0,
    MGMT_MSG_ERR_BADMSG = 1,
    MGMT_MSG_ERR_EINVAL = 2,
};
```

### Bad

```c
#define MGMT_MSG_ERR_BADMSG -74   /* Linux errno EBADMSG value */
```

## Scope of this file

Path glob: `lib/mgmt_*.h`. This is strictly tighter than
`lib-headers.instructions.md` (which applies to all `lib/**/*.h`).
Both files' rules apply to `lib/mgmt_*.h` files in aggregate.
