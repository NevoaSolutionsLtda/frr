#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate bgpd northbound stubs from a missing-callbacks TSV.

Input format (one line per missing callback):
    <op>\\t<xpath>

Where <op> is one of: create, modify, destroy, get_next, get_keys,
lookup_entry.

Output:
    bgpd/bgp_nb_stubs_table.inc -- nodes-table fragment, suitable for
        #include inside the .nodes = { ... } initializer of
        frr_bgp_info in bgpd/bgp_nb.c.

Stub classes (NevoaSolutionsLtda/frr issue #39, Phase D):

    reject (default) -- every stubbed node that is neither allowlisted
        nor a structural ancestor. Config callbacks bind reject-strict
        stubs: a programmatic (non-CLI) commit fails validation with an
        explicit error instead of returning a false commit-OK. This is
        the wired-or-reject policy: an unwired knob must fail loudly
        until it gets a real callback in bgp_nb.c.

    warn -- documented exceptions, NB_OK no-ops that emit an aggregated
        log warning for programmatic writes. Three families:

        * structural (computed): stubbed ancestors of a wired (or
          allowlisted) node. Creating the wired descendant implicitly
          creates these nodes in the same commit, so rejecting their
          create would veto legitimate wired writes. The wired set is
          extracted from bgpd/bgp_nb.c (literal .xpath initializers
          plus the BGP_NB_*_XPATH() string-concatenation macros), so
          the classification follows the code automatically.

        * finisher children (computed): direct children of a wired
          node that carries .apply_finish. The finisher reads the
          leaf values from the datastore, so the child stubs are the
          working design (local-as, bfd-options, med-config, ...),
          not unwired debt.

        * allowlist (manual, below): subtree prefixes kept as warn by
          an explicit policy decision. The bar for an entry: the
          programmatic write MUST be tolerable as a datastore-only
          no-op (the legacy CLI remains the configuration authority)
          AND there must be a concrete reason wired-or-reject cannot
          hold yet. The reason string is mandatory and is printed in
          the generated table.

Oper callbacks (get_next, get_keys, lookup_entry) stay neutral no-ops
for both classes. The CLI dual-write path is exempt from rejection and
warning inside the stub implementations (bgpd/bgp_nb_stubs.c).

Validation (hard errors, so a stale input cannot silently downgrade
the policy):
    - a TSV xpath that also has callbacks in bgp_nb.c (remove it from
      the TSV and regenerate);
    - an allowlist entry matching no stubbed xpath;
    - a BGP_NB_*_XPATH() usage the expander cannot resolve.

The stub function symbols themselves live in bgpd/bgp_nb_stubs.c and
are declared in bgpd/bgp_nb_stubs.h; this generator only emits the
table entries.

Idempotent: rerun whenever the YANG tree changes and the diff appears
in the .inc file.
"""
import collections
import os
import re
import sys

STUB_OPS = ("create", "modify", "destroy", "get_next", "get_keys",
            "lookup_entry")

WARN_OP_TO_CB = {
    "create": "bgp_nb_stub_create",
    "modify": "bgp_nb_stub_modify",
    "destroy": "bgp_nb_stub_destroy",
    "get_next": "bgp_nb_stub_get_next",
    "get_keys": "bgp_nb_stub_get_keys",
    "lookup_entry": "bgp_nb_stub_lookup_entry",
}

# Config ops overridden for the reject class; oper ops fall back to the
# neutral stubs above.
REJECT_OP_TO_CB = {
    "create": "bgp_nb_stub_reject_create",
    "modify": "bgp_nb_stub_reject_modify",
    "destroy": "bgp_nb_stub_reject_destroy",
}

# Documented warn allowlist: (subtree prefix, reason). A stub xpath
# equal to the prefix or nested under it stays warn. Keep EMPTY unless
# a policy decision is recorded here -- see the module docstring for
# the bar an entry must clear.
#
# Context- and instance-creation leaves whose fanouts are not wired
# yet: the numbered-neighbor twin of each is wired, but a programmatic
# commit must carry these leaves in the same transaction that creates
# the peer-group / unnumbered-neighbor / AF / BGP instance, so
# reject-strict would veto the whole context creation (the S061
# peer-group gate, the EVPN unnumbered fanout and VRF gates, and the
# mgmt set-config round-trip alike). Warn keeps the aggregated
# warning for the datastore-only no-op; wiring the fanouts stays
# tracked as the Phase D follow-up of issue #39.
_BGP = ("/frr-routing:routing/control-plane-protocols/"
        "control-plane-protocol/frr-bgp:bgp")
ALLOWLIST = [
    (_BGP + "/peer-groups/peer-group/afi-safis/afi-safi/enabled",
     "AF-activation leaf of the unwired fanout contexts (issue #39)"),
    (_BGP + "/neighbors/unnumbered-neighbor/afi-safis/afi-safi/enabled",
     "AF-activation leaf of the unwired fanout contexts (issue #39)"),
    (_BGP + "/global/local-as",
     "BGP-instance leaf: legacy boot mirrors only the default "
     "instance; programmatic instance creation (e.g. a VRF) needs it "
     "in the same transaction (issue #39)"),
]

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_NB_C = os.path.join(REPO_ROOT, "bgpd", "bgp_nb.c")

XPATH_MACRO_RX = re.compile(r"\bBGP_NB_\w*XPATH\s*\(")


def unescape(s: str) -> str:
    return s.replace('\\"', '"').replace("\\\\", "\\")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def parse_xpath_macros(text: str):
    """Return {name: (params, tokens)} for the BGP_NB_*_XPATH macros.

    tokens is the concatenation sequence of ('lit', s) and
    ('param', name) pieces. Anything that is not a pure string
    concatenation is a hard error: the expander must never guess.
    """
    text = re.sub(r"\\\s*\n", " ", text)
    macros = {}
    spans = []
    for m in re.finditer(
        r"^#define\s+(BGP_NB_\w*XPATH)\s*\(([^)]*)\)(.*)$",
        text, re.M,
    ):
        name = m.group(1)
        params = [p.strip() for p in m.group(2).split(",")]
        toks = []
        for t in re.finditer(r'"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*',
                             m.group(3)):
            s = t.group(0)
            if s.startswith('"'):
                toks.append(("lit", unescape(s[1:-1])))
            elif s in params:
                toks.append(("param", s))
            else:
                sys.exit(f"error: {name} is not a pure string "
                         f"concatenation (token {s!r})")
        macros[name] = (params, toks)
        spans.append(m.span())
    for start, end in spans:
        text = text[:start] + " " * (end - start) + text[end:]
    return macros, text


def call_args(text: str, open_paren: int):
    """Split the balanced argument list starting at text[open_paren]=='('."""
    depth = 0
    i = open_paren
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                break
        i += 1
    inner = text[open_paren + 1:i]
    args, buf, depth, in_str = [], [], 0, False
    for ch in inner:
        if in_str:
            buf.append(ch)
            if ch == '"':
                in_str = False
            continue
        if ch == '"':
            in_str = True
            buf.append(ch)
        elif ch in "([":
            depth += 1
            buf.append(ch)
        elif ch in ")]":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            args.append("".join(buf).strip())
            buf = []
        else:
            buf.append(ch)
    tail = "".join(buf).strip()
    if tail:
        args.append(tail)
    return args


def wired_xpaths(nb_c_path: str) -> dict:
    """Map each xpath with callbacks in bgpd/bgp_nb.c to whether its
    callback set includes .apply_finish (macros expanded)."""
    with open(nb_c_path) as fh:
        text = strip_comments(fh.read())

    macros, text = parse_xpath_macros(text)
    entries = []
    for m in re.finditer(r'\.xpath\s*=\s*"((?:[^"\\]|\\.)*)"', text):
        entries.append((m.start(), unescape(m.group(1))))
    for m in XPATH_MACRO_RX.finditer(text):
        name = m.group(0).rstrip("(")
        if name not in macros:
            sys.exit(f"error: unexpandable macro usage {name} in "
                     f"{nb_c_path}")
        params, toks = macros[name]
        args = call_args(text, m.end() - 1)
        if len(args) != len(params):
            sys.exit(f"error: {name} expects {len(params)} arg(s), "
                     f"got {args}")
        sub = {}
        for param, arg in zip(params, args):
            if not (arg.startswith('"') and arg.endswith('"')):
                sys.exit(f"error: {name} argument {arg!r} is not a "
                         f"string literal")
            sub[param] = unescape(arg[1:-1])
        entries.append((m.start(),
                        "".join(v if k == "lit" else sub[v]
                                for k, v in toks)))
    entries.sort()

    wired = {}
    for i, (pos, xpath) in enumerate(entries):
        end = entries[i + 1][0] if i + 1 < len(entries) else len(text)
        wired[xpath] = ".apply_finish" in text[pos:end]
    return wired


def classify(by_xpath, structural):
    def klass(xpath):
        kind = structural.get(xpath)
        if kind:
            return kind
        for prefix, _reason in ALLOWLIST:
            if xpath == prefix or xpath.startswith(prefix + "/"):
                return "warn-allowlist"
        return "reject"

    return {xp: klass(xp) for xp in by_xpath}


def main(tsv_path: str, out_path: str, nb_c_path: str) -> int:
    by_xpath: dict[str, set[str]] = collections.defaultdict(set)
    with open(tsv_path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or not line.startswith(tuple(
                    f"{op}\t" for op in STUB_OPS)):
                continue
            op, xpath = line.split("\t", 1)
            by_xpath[xpath].add(op)

    if not by_xpath:
        print(f"no stub entries parsed from {tsv_path}", file=sys.stderr)
        return 1

    wired = wired_xpaths(nb_c_path)

    overlap = sorted(set(by_xpath) & set(wired))
    if overlap:
        for xp in overlap[:10]:
            print(f"error: {xp} has callbacks in {nb_c_path}; "
                  f"remove it from {tsv_path}", file=sys.stderr)
        return 1

    allowlisted = set()
    for prefix, _reason in ALLOWLIST:
        hits = {xp for xp in by_xpath
                if xp == prefix or xp.startswith(prefix + "/")}
        if not hits:
            print(f"error: allowlist entry {prefix!r} matches no "
                  f"stubbed xpath", file=sys.stderr)
            return 1
        allowlisted |= hits

    # structural: stubbed ancestors of a node a legitimate programmatic
    # write may create (wired or allowlisted), and direct children of
    # wired nodes that carry .apply_finish -- the finisher reads the
    # datastore, so the child stubs are the design, not debt.
    targets = set(wired) | allowlisted
    finishers = {xp for xp, has in wired.items() if has}
    structural = {}
    for p in by_xpath:
        if any(t.startswith(p + "/") for t in targets):
            structural[p] = "warn-structural"
        elif p.rsplit("/", 1)[0] in finishers:
            structural[p] = "warn-finisher-child"

    classes = classify(by_xpath, structural)
    n_reject = sum(1 for k in classes.values() if k == "reject")
    n_struct = sum(1 for k in structural.values()
                   if k == "warn-structural")
    n_finish = sum(1 for k in structural.values()
                   if k == "warn-finisher-child")
    n_allow = len(allowlisted - set(structural))

    lines = []
    lines.append("/* SPDX-License-Identifier: GPL-2.0-or-later */")
    lines.append("/*")
    lines.append(" * Auto-generated by tools/gen-bgp-nb-stubs.py.")
    lines.append(" * Do not edit by hand. Regenerate by running:")
    lines.append(" *   python3 tools/gen-bgp-nb-stubs.py "
                 "tools/missing_cbs.tsv \\")
    lines.append(" *     bgpd/bgp_nb_stubs_table.inc")
    lines.append(" *")
    lines.append(" * Each entry binds a YANG node to stub callbacks so that")
    lines.append(" * nb_validate_callbacks() passes for the frr-bgp module")
    lines.append(" * tree. The trailing comment is the stub class column")
    lines.append(" * (issue #39 Phase D, wired-or-reject policy):")
    lines.append(" * reject = programmatic writes fail validation,")
    lines.append(" * warn = NB_OK no-op; structural ancestors of wired")
    lines.append(" * nodes, children of wired apply_finish containers,")
    lines.append(" * and the documented ALLOWLIST in the generator.")
    lines.append(f" * Current population: {n_reject} reject, "
                 f"{n_struct} warn-structural, "
                 f"{n_finish} warn-finisher-child, "
                 f"{n_allow} warn-allowlist.")
    lines.append(" *")
    lines.append(" * Real handlers for any of these xpaths should be")
    lines.append(" * added to bgp_nb.c (above the #include of this file)")
    lines.append(" * and the corresponding line removed from this file or")
    lines.append(" * from tools/missing_cbs.tsv before regeneration.")
    lines.append(" */")
    lines.append("")
    for xpath in sorted(by_xpath):
        ops = by_xpath[xpath]
        klass = classes[xpath]
        cb_lines = []
        for op in sorted(ops):
            if klass == "reject" and op in REJECT_OP_TO_CB:
                cb = REJECT_OP_TO_CB[op]
            else:
                cb = WARN_OP_TO_CB[op]
            cb_lines.append(f".{op} = {cb},")
        cb_block = " ".join(cb_lines)
        if klass == "warn-structural":
            comment = "warn (structural ancestor of a wired node)"
        elif klass == "warn-finisher-child":
            comment = ("warn (child of a wired apply_finish container)")
        elif klass == "warn-allowlist":
            reason = next(r for p, r in ALLOWLIST
                          if xpath == p or xpath.startswith(p + "/"))
            comment = f"warn (allowlist: {reason})"
        else:
            comment = "reject"
        lines.append(f'{{ .xpath = "{xpath}",')
        lines.append(f'  .cbs = {{ {cb_block} }} }}, /* class: {comment} */')

    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(out_path, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    print(f"wrote {out_path}: {len(by_xpath)} xpath stub entries "
          f"({n_reject} reject, {n_struct} warn-structural, "
          f"{n_finish} warn-finisher-child, "
          f"{n_allow} warn-allowlist)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        print(f"usage: {sys.argv[0]} <missing_cbs.tsv> <out.inc> "
              f"[<bgp_nb.c>]", file=sys.stderr)
        sys.exit(2)
    nb_c = sys.argv[3] if len(sys.argv) == 4 else DEFAULT_NB_C
    sys.exit(main(sys.argv[1], sys.argv[2], nb_c))
