# SPDX-License-Identifier: ISC
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
#
# Copyright (C) 2026  Reinaldo Saraiva
#

"""
Test the gRPC transaction-history readers (org issue #29).

ListTransactions must reflect the real mgmtd commit history -- no fake
entries, an exact count delta per bridge commit -- and GetTransaction
must return the recorded config of a listed id and refuse unknown ids.

The I2 follow-up adds the writer side: CommitResponse.transaction_id
must carry the history id of the commit it just recorded (the id that
ListTransactions reports for the newest entry).
"""

import glob
import json
import os

import pytest
from lib.common_config import step
from lib.topogen import Topogen, TopoRouter

CWD = os.path.dirname(os.path.realpath(__file__))
GRPCP_MGMTD = 50065
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.mgmtd]

DESC_XPATH = "/frr-interface:lib/interface[name='r1-eth0']/description"
FAKE_IDS = (0xFFFF, 0xFFFE)
UNKNOWN_ID = 4294967295


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
    from lib.micronet import commander

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


def _list_transactions(r):
    output = run_grpc_client(r, "list-transactions-full,5")
    return json.loads(output.strip().splitlines()[-1])


def _get_transaction(r, transaction_id):
    output = run_grpc_client(r, f"get-transaction,{transaction_id},1,0")
    return json.loads(output.strip().splitlines()[-1])


def test_history_reflects_real_commits(tgen):
    r1 = tgen.gears["r1"]

    step("Snapshot the transaction list before the commits")
    before = _list_transactions(r1)

    step("Commit three distinct values through the gRPC bridge")
    for i in range(3):
        run_grpc_client(r1, f"commit-set,{DESC_XPATH}=s054-hist-{i}")

    step("ListTransactions reports the real commits and no fakes")
    after = _list_transactions(r1)
    ids = [entry["id"] for entry in after]

    assert all(fake not in ids for fake in FAKE_IDS)
    assert all("fake" not in entry["client"] for entry in after)
    # delta by ids (not by length): the ring caps the list at 10, so a
    # full initial snapshot would hide new commits behind evictions.
    new_ids = [i for i in ids if i not in set(e["id"] for e in before)]
    assert len(new_ids) == 3
    assert all(entry["client"] and entry["date"] for entry in after)

    step("GetTransaction returns the recorded config of a listed id")
    got = _get_transaction(r1, ids[0])
    assert "error" not in got
    assert "s054-hist-2" in got["config"]

    step("GetTransaction refuses an id that is not in the history")
    missing = _get_transaction(r1, UNKNOWN_ID)
    assert missing.get("error") == "INVALID_ARGUMENT"

    step("Clean up the test description")
    run_grpc_client(r1, f"commit-delete,{DESC_XPATH}")

    step("CommitResponse carries the history id of its own commit (I2)")
    before_i2 = _list_transactions(r1)
    out = run_grpc_client(r1, f"commit-result,ALL,{DESC_XPATH}=s056-i2")
    resp = json.loads(out.strip().splitlines()[-1])
    after_i2 = _list_transactions(r1)

    assert resp["status"] == "OK"
    assert resp["transaction_id"] != 0, (
        f"CommitResponse.transaction_id not filled: {resp}"
    )
    assert after_i2[0]["id"] == resp["transaction_id"]
    new_i2 = [
        e["id"]
        for e in after_i2
        if e["id"] not in {x["id"] for x in before_i2}
    ]
    assert new_i2 == [resp["transaction_id"]]

    step("A no-change commit is refused and records no history entry")
    out0 = run_grpc_client(r1, f"commit-result,ALL,{DESC_XPATH}=s056-i2")
    resp0 = json.loads(out0.strip().splitlines()[-1])
    after0 = _list_transactions(r1)

    assert resp0["status"] == "ABORTED"
    assert "No changes" in resp0["details"]
    assert not resp0.get("transaction_id")
    assert [
        e["id"] for e in after0 if e["id"] not in {x["id"] for x in after_i2}
    ] == []

    step("Clean up the I2 description")
    run_grpc_client(r1, f"commit-delete,{DESC_XPATH}")
