---
applyTo: "doc/user/**"
---

# doc/user/** discipline

## P2 — DOC block length cap ~30 lines

Flag any new or expanded RST (or Markdown, if any) block in
`doc/user/**` that exceeds roughly 30 lines of inline prose in a
single burst.

donaldsharp rejected a 59-line DOC block in PR #21557 as "stupidly
over the top." Long inline documentation is expensive to maintain:
it rots, it gets stale against code, and it makes the patch diff
hard to review.

Preferred shapes:

1. **Link to `docs.frrouting.org`** for conceptual material. The
   website has navigation, search, and cross-linking; plain text in
   the repo does not.
2. **Keep rationale in the commit message.** Reviewers look at
   `git log` when reading code. Rationale in the commit message is
   easy to find via `git blame` and `git log -p`.
3. **Short, pointed comment + link** in the doc itself. One
   sentence of context, then a URL.

## When a long block IS justified

- Release notes with itemised user-visible changes — those need
  length.
- A new CLI command's help text that documents every flag — that is
  reference material and belongs in the command's section.
- A migration guide for a specific NBC break — that has inherent
  length.

In those cases, the commit body MUST explain why the length is
necessary and why it cannot be offloaded to docs.frrouting.org.

## P3 — RST anti-patterns

- Multi-level bullets nested past three levels — flag and suggest
  flattening.
- Code blocks longer than 40 lines — flag and suggest linking to a
  GitHub permalink of the real source instead.
- ASCII-art diagrams of topologies that could be a PNG or SVG —
  flag with a comment noting the repo already has image support.

## Scope of this file

Path glob: `doc/user/**`. For DOC comments embedded in code
(Doxygen-style), the repo-wide `copilot-instructions.md` tone rules
apply, not this file.
