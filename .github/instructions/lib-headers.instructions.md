---
applyTo: "lib/**/*.h"
---

# lib/ public header discipline

## P1 — do NOT remove a user-configure `#ifdef HAVE_*` guard from a public struct member

Flag any change that takes a struct member guarded by
`#ifdef HAVE_SQLITE3`, `#ifdef HAVE_LIBPCRE2_POSIX`, `#ifdef HAVE_SYSREPO`,
or any other user-visible configure-time option, and makes the member
unconditional (removes the guard).

FRR's build model expects all components compiled from the same
`config.h`. "Struct layout stability across configure flags" is not a
supported use case. Removing the guard to stabilise offsets silently
breaks external clients built with a mismatched `config.h` — they
will end up with wrong offsets into the struct.

Reference: PR #21538 wontfix, choppsv1 NACK 2026-04-21.

### Allowed category — OS-feature-detection guards

The rule above is **not** about OS-feature-detection guards such as
`HAVE_PROC_NET_DEV`, `HAVE_NET_RT_IFLIST`, `HAVE_NETNS`. Those can
reasonably be normalised or removed because they describe the build
host, not a user knob. Do not flag changes that touch only those.

If in doubt, check whether the guard corresponds to a `configure`
option mentioned in `doc/developer/building-*.rst` or similar docs —
if yes, it is user-visible and falls under the P1 rule.

## P2 — do NOT re-document standard library calls in headers

Flag any new inline comment in a public header that restates the
semantics of a standard library call: `listen()`, `read()`, `write()`,
`memcpy()`, `memset()`, `strlen()`, `open()`, `close()`, `select()`,
`poll()`, `malloc()`, `free()`, etc.

Reviewers expect readers to know libc. Comments should explain why
the call is here or what invariant it preserves, not what the call
does.

Reference: PR #21514 — choppsv1 requested removal of a `listen()`
explanatory comment.

### Acceptable comment shapes

- "Must be called before worker fork" — a lifecycle constraint.
- "Handles EAGAIN by retry; EINTR by abort" — behaviour that is not
  in the man page.
- "TODO(#issue) — replace with async variant once #NNNNN lands" — a
  scoped future action.

### Not acceptable

- "listen() marks the socket as passive and sets the backlog"
- "read() returns 0 on EOF and -1 on error"
- "memcpy() copies `n` bytes from `src` to `dst`"

## Scope of this file

Path glob: `lib/**/*.h`. For rules on `lib/mgmt_*.h` specifically,
see `mgmt-wire.instructions.md`.
