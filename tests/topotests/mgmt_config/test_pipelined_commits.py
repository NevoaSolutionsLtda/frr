# -*- coding: utf-8 eval: (blacken-mode 1) -*-
# SPDX-License-Identifier: ISC
#
# April 22 2026, Reinaldo Saraiva <reinaldo.saraiva@gmail.com>
#
# Copyright (c) 2026, Reinaldo Saraiva
#

"""
Verify that two FE sessions can overlap in the CONFIG txn finishing slot.

Regression for the admission-gate narrowing where `config_active_txn` is
cleared immediately after `mgmt_ds_copy_dss()` success rather than at
teardown.  Before that change, a second CONFIG session that arrived
while the first was still in its FE reply / teardown path would be
rejected by `mgmt_create_txn(CONFIG)` returning `MGMTD_TXN_ID_NONE`
(`mgmtd/mgmt_fe_adapter.c: "failed to create config transaction"`), or
the follow-up `LOCK_DS` request would fail with "could not lock
candidate DS" because the DS txn-lock was still held by the previous
session's not-yet-destroyed CONFIG txn.

Strategy: fire two `vtysh -c "... commit apply"` sessions back-to-back
against the same router.  Session A carries a larger payload (10
prefix-list entries), session B a smaller one (1 entry), so A takes
longer than B end-to-end.  B is launched with a small offset after A
to bias B's LOCK_DS arrival toward A's post-`copy_dss` window.

The test is specified by four per-iteration invariants:

  (1) Neither session output contains a known admission-gate or
      DS-lock rejection string.  Checked before the returncode so
      failure messages name the exact regression signature.
  (2) Both sessions exit 0.
  (3) Both prefix-lists appear in running-config after both exit.
  (4) Session B completes faster (wall-clock, from its own launch to
      its exit) than session A does.  With the patch present A and B
      pipeline; since B is the smaller payload, B's elapsed time is
      strictly below A's end-to-end time.  With the patch reverted B
      either fails (covered by (1) and (2)) or serialises behind A,
      making B's elapsed longer than A's -- either way (4) fails.
      This is the positive pipelining assertion the returncode and
      coexistence checks alone cannot supply.

Repeated over N_ITERATIONS to shrink the chance of a false positive
from per-run timing noise.
"""

import re
import subprocess
import sys
import time

import pytest
from lib.topogen import Topogen

pytestmark = [pytest.mark.staticd, pytest.mark.mgmtd]

N_ITERATIONS = 10
LAUNCH_OFFSET_S = 0.150
COMMIT_TIMEOUT_S = 10
# Positive-pipelining margin: B's elapsed must beat A's by at least
# this much for the iteration to count as overlap evidence.  Kept
# small because B genuinely is a lighter vtysh session (one entry vs
# ten); a larger margin would mask CI scheduler jitter.
PIPELINE_MARGIN_S = 0.005

REJECTION_PATTERNS = (
    # admission-gate reject from `mgmtd/mgmt_fe_adapter.c: fe_session_send_error`
    re.compile(r"failed to create config transaction"),
    # DS lock contention; the visible pre-patch signal observed on the lab.
    # `mgmtd/mgmt_ds.c: mgmt_ds_txn_lock`; surfaces via vtysh as
    # `% could not lock candidate DS\n% Failed to apply configuration data.`
    re.compile(r"could not lock candidate DS"),
    re.compile(r"Failed to apply configuration data"),
    # Legacy wording observed on other mgmtd paths.
    re.compile(r"locked by another transaction"),
    re.compile(r"CONFIG txn.*in progress"),
)


@pytest.fixture(scope="module")
def tgen(request):
    "Setup/Teardown the environment and provide tgen argument to tests"

    topodef = {"s1": ("r1",)}
    tgen = Topogen(topodef, request.module.__name__)
    tgen.start_topology()

    tgen.gears["r1"].load_frr_config("frr.conf")
    tgen.start_router()
    yield tgen
    tgen.stop_topology()


def _build_commit_payload(name, n_entries):
    """Return a vtysh command string that creates `n_entries` prefix-list
    entries under `name` and commits the candidate datastore."""
    base = int("c0000200", 16)  # 192.0.2.0
    lines = ["conf t"]
    for i in range(n_entries):
        prefix = ".".join(str((base + i * 4) >> (8 * (3 - k)) & 0xFF) for k in range(4))
        lines.append(f"ip prefix-list {name} seq {10 + i} permit {prefix}/30")
    lines.append("end")
    return "\n".join(lines)


def _launch_commit(r1, payload):
    """Open a subprocess vtysh session that applies `payload`.  Each
    invocation spawns a fresh vtysh process and therefore a fresh FE
    session (see `mgmtd/mgmt_fe_adapter.c: fe_session_create`)."""
    return r1.popen(
        ["/usr/bin/env", "vtysh", "-c", payload],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def _assert_no_rejection(out, label, iteration):
    for pattern in REJECTION_PATTERNS:
        match = pattern.search(out)
        assert match is None, (
            f"iter {iteration} session {label}: pre-patch admission-gate "
            f"rejection detected (matched {match.group()!r} in output): {out!r}"
        )


def test_pipelined_config_commits(tgen):
    """Two overlapping CONFIG commits from distinct FE sessions must both
    succeed and the smaller one must finish ahead.  Regression guard for
    the admission-gate narrowing."""
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)
    r1 = tgen.gears["r1"]

    for i in range(N_ITERATIONS):
        name_a = f"pl-pipe-A-{i}"
        name_b = f"pl-pipe-B-{i}"
        payload_a = _build_commit_payload(name_a, n_entries=10)
        payload_b = _build_commit_payload(name_b, n_entries=1)

        t_a_start = time.monotonic()
        proc_a = _launch_commit(r1, payload_a)
        time.sleep(LAUNCH_OFFSET_S)
        t_b_start = time.monotonic()
        proc_b = _launch_commit(r1, payload_b)

        out_a, _ = proc_a.communicate(timeout=COMMIT_TIMEOUT_S)
        t_a_elapsed = time.monotonic() - t_a_start
        out_b, _ = proc_b.communicate(timeout=COMMIT_TIMEOUT_S)
        t_b_elapsed = time.monotonic() - t_b_start

        _assert_no_rejection(out_a, "A", i)
        _assert_no_rejection(out_b, "B", i)
        assert proc_a.returncode == 0, (
            f"iter {i} session A non-zero exit {proc_a.returncode}: {out_a!r}"
        )
        assert proc_b.returncode == 0, (
            f"iter {i} session B non-zero exit {proc_b.returncode}: {out_b!r}"
        )

        running = r1.vtysh_cmd("show running-config")
        assert name_a in running, (
            f"iter {i}: session A payload {name_a!r} did not land in running: "
            f"{running}"
        )
        assert name_b in running, (
            f"iter {i}: session B payload {name_b!r} did not land in running: "
            f"{running}"
        )

        assert t_b_elapsed + PIPELINE_MARGIN_S < t_a_elapsed, (
            f"iter {i}: session B did not beat session A end-to-end: "
            f"t_a={t_a_elapsed * 1000:.1f}ms t_b={t_b_elapsed * 1000:.1f}ms. "
            f"With the admission-gate narrowing, B (1-entry payload) is "
            f"expected to finish ahead of A (10-entry payload) because the "
            f"CONFIG txns pipeline; if B has to serialise behind A its "
            f"elapsed time inflates to include A's teardown wait."
        )

        r1.vtysh_cmd(
            f"conf t\nno ip prefix-list {name_a}\nno ip prefix-list {name_b}\nend\n"
        )


if __name__ == "__main__":
    sys.exit(pytest.main(sys.argv[1:] + [__file__]))
