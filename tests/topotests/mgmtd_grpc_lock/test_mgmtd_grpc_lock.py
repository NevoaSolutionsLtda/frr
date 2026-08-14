# SPDX-License-Identifier: ISC
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
#
# Copyright (C) 2026  Reinaldo Saraiva
#

"""
Test mgmtd gRPC config-lock ownership and lease (org issue #28).

The gRPC LockConfig/UnlockConfig pair must share one lock domain with
the mgmtd datastores, owned per channel: a lock excludes commits and
unlocks from every other channel, the owner still commits under its own
lock, and the lease is the owning channel's life -- a channel that dies
without unlocking must not leave the lock behind.
"""

import glob
import json
import os

import pytest
from lib.common_config import step
from lib.micronet import commander
from lib.topogen import Topogen, TopoRouter

CWD = os.path.dirname(os.path.realpath(__file__))
GRPCP_MGMTD = 50063
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.mgmtd]

DESC_XPATH = "/frr-interface:lib/interface[name='r1-eth0']/description"
DESC_OWNER = "s053-owner-applied"
DESC_INTRUDER = "s053-intruder-blocked"


def _frr_grpc_module_available():
    """True when the FRR northbound gRPC module (grpc.so) is installed."""
    patterns = (
        "/usr/lib/*/frr/modules/grpc.so",
        "/usr/lib/frr/modules/grpc.so",
        "/usr/lib64/*/frr/modules/grpc.so",
        "/usr/lib64/frr/modules/grpc.so",
        "/usr/local/lib/*/frr/modules/grpc.so",
        "/usr/local/lib/frr/modules/grpc.so",
    )
    for pattern in patterns:
        for path in glob.glob(pattern):
            if os.path.isfile(path):
                return True

    frr_root = os.path.realpath(os.path.join(CWD, "../../.."))
    for base in (frr_root, os.environ.get("FRR_BUILD_DIR")):
        if not base:
            continue
        for rel in ("lib/.libs/grpc.so", "lib/grpc.so"):
            if os.path.isfile(os.path.join(base, rel)):
                return True
    return False


try:
    import grpc  # noqa: F401
    import grpc_tools  # noqa: F401
except ImportError:
    pytest.skip("skipping; gRPC modules not installed", allow_module_level=True)

if not _frr_grpc_module_available():
    pytest.skip(
        "skipping; FRR gRPC northbound module not installed "
        "(install frr-grpc or build with --enable-grpc)",
        allow_module_level=True,
    )

try:
    commander.cmd_raises([script_path, "--check"])
except Exception:
    pytest.skip(
        "skipping; cannot create or import gRPC proto modules",
        allow_module_level=True,
    )


@pytest.fixture(scope="module")
def tgen(request):
    "Setup/Teardown the environment and provide tgen argument to tests"

    topodef = {"s1": ("r1",)}
    tgen = Topogen(topodef, request.module.__name__)
    tgen.start_topology()

    router = tgen.gears["r1"]
    router.load_frr_config("frr.conf")
    router.load_config(TopoRouter.RD_MGMTD, "", f"-M grpc:{GRPCP_MGMTD}")

    tgen.start_router()
    yield tgen
    tgen.stop_topology()


@pytest.fixture(autouse=True)
def skip_on_failure(tgen):
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)


def run_grpc_client(r, commands, extra_args=None):
    if not isinstance(commands, str):
        commands = "\n".join(commands) + "\n"
    if not commands.endswith("\n"):
        commands += "\n"
    args = [script_path, f"--port={GRPCP_MGMTD}"]
    if extra_args:
        args.extend(extra_args)
    return r.cmd_raises(args, stdin=commands)


def test_lock_ownership_excludes_other_channels(tgen):
    r1 = tgen.gears["r1"]

    step("Run the two-channel lock ownership scenario")
    output = run_grpc_client(
        r1,
        f"LOCK-OWNERSHIP-SCENARIO,{DESC_XPATH},{DESC_OWNER},{DESC_INTRUDER}",
    )
    results = json.loads(output.strip().splitlines()[-1])

    step("The owner locks; every foreign operation is refused")
    assert results["lock_a"] == "OK"
    # lock_b is refused on the pre-fix base too (single anonymous slot);
    # the assertions that discriminate the fix are unlock_b, commit_b
    # and the lease scenario below.
    assert results["lock_b_while_a"] == "FAILED_PRECONDITION"
    assert results["unlock_b_while_a"] == "FAILED_PRECONDITION"
    assert results["commit_b_while_a"] == "FAILED_PRECONDITION"

    step("The owner still commits under its own lock and can unlock")
    assert results["commit_a_self_lock"] == "OK"
    assert results["unlock_a"] == "OK"

    step("After the owner unlocks, another channel gets the lock")
    assert results["lock_b_after"] == "OK"
    assert results["unlock_b_after"] == "OK"

    step("Only the owner's value reached the running config")
    running = r1.vtysh_cmd("show running-config")
    assert DESC_OWNER in running
    assert DESC_INTRUDER not in running

    step("Clean up the test description")
    run_grpc_client(r1, f"COMMIT-DELETE,{DESC_XPATH}")


def test_lock_lease_released_on_channel_death(tgen):
    r1 = tgen.gears["r1"]

    step("Lock from a channel that dies without unlocking")
    output = run_grpc_client(r1, "LOCK-LEASE-SCENARIO,10")
    results = json.loads(output.strip().splitlines()[-1])

    assert results["lock_c"] == "OK"

    step("A new channel gets the lock once the death is noticed")
    assert results["lock_d"] == "OK", (
        "lock leaked after channel death: %s" % results
    )
    assert results["takeover_secs"] >= 0
