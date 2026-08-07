# SPDX-License-Identifier: ISC
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
#
# Copyright (C) 2026  Eric Parsonage
#

"""
Test mgmtd gRPC Get dispatch of backend operational state.
"""

import glob
import json
import os

import pytest
from lib.common_config import retry, step
from lib.micronet import commander
from lib.topogen import Topogen, TopoRouter

CWD = os.path.dirname(os.path.realpath(__file__))
GRPCP_MGMTD = 50061
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.mgmtd]


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

    topodef = {"s1": ("r1", "r2")}
    tgen = Topogen(topodef, request.module.__name__)
    tgen.start_topology()

    for rname, router in tgen.routers().items():
        router.load_frr_config("frr.conf")
        if rname == "r1":
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


def run_grpc_client_status(r, commands, port=GRPCP_MGMTD):
    if not commands.endswith("\n"):
        commands += "\n"
    return r.net.cmd_status([script_path, f"--port={port}"], stdin=commands)


@retry(retry_timeout=60)
def check_backend_oper_served(r1):
    try:
        output = run_grpc_client(r1, "GET-STATE,/frr-interface:lib")
    except Exception as error:
        return f"gRPC get-state failed: {error}"
    if '"if-index"' not in output:
        return "no operational if-index in get-state output"
    return None


def interfaces_by_name(output):
    out_json = json.loads(output)
    interfaces = out_json["frr-interface:lib"]["interface"]
    return {i["name"]: i for i in interfaces}


def test_get_state_backend_interface(tgen):
    r1 = tgen.gears["r1"]

    step("Read zebra-owned interface state through mgmtd gRPC Get(STATE)")
    check_backend_oper_served(r1)

    output = run_grpc_client(r1, "GET-STATE,/frr-interface:lib")
    byname = interfaces_by_name(output)
    assert "r1-eth0" in byname
    assert "if-index" in byname["r1-eth0"]["state"]


def test_get_all_merges_config_and_state(tgen):
    r1 = tgen.gears["r1"]

    step("Read merged config and state through mgmtd gRPC Get(ALL)")
    check_backend_oper_served(r1)

    output = run_grpc_client(r1, "GET,/frr-interface:lib")
    byname = interfaces_by_name(output)
    assert "r1-eth0" in byname
    r1_eth0 = byname["r1-eth0"]

    # State merged from the zebra backend.
    assert "if-index" in r1_eth0["state"]

    # Config merged from the running datastore.
    addrs = r1_eth0["frr-zebra:zebra"]["ipv4-addrs"]
    assert any(a["ip"] == "192.0.2.1" for a in addrs)


def test_get_state_repeated(tgen):
    r1 = tgen.gears["r1"]

    step("Read backend state repeatedly to exercise per-request transactions")
    check_backend_oper_served(r1)

    for _ in range(5):
        output = run_grpc_client(r1, "GET-STATE,/frr-interface:lib")
        byname = interfaces_by_name(output)
        assert "r1-eth0" in byname


def test_get_state_invalid_path(tgen):
    r1 = tgen.gears["r1"]

    step("An unresolvable path still fails the Get(STATE) request")
    status, _, _ = run_grpc_client_status(r1, "GET-STATE,/frr-bogus:nope")
    assert status != 0


def test_get_state_valid_path_without_backend(tgen):
    r1 = tgen.gears["r1"]

    step("A valid path with no owning backend yields an empty result, not an error")
    output = run_grpc_client(r1, "GET-STATE,/frr-ripd:ripd")
    assert output.strip() == ""


def test_get_config_regression(tgen):
    r1 = tgen.gears["r1"]

    step("CONFIG reads keep using the synchronous local path")
    output = run_grpc_client(r1, "GET-CONFIG,/frr-interface:lib")
    byname = interfaces_by_name(output)
    addrs = byname["r1-eth0"]["frr-zebra:zebra"]["ipv4-addrs"]
    assert any(a["ip"] == "192.0.2.1" for a in addrs)


def test_get_root_all(tgen):
    r1 = tgen.gears["r1"]

    step("A root Get(ALL) dumps merged config and backend state")
    check_backend_oper_served(r1)

    output = run_grpc_client(r1, "GET,/")
    out_json = json.loads(output)
    interfaces = out_json["frr-interface:lib"]["interface"]
    byname = {i["name"]: i for i in interfaces}
    assert "r1-eth0" in byname

    # State merged from the zebra backend.
    assert "if-index" in byname["r1-eth0"]["state"]

    # Config merged from the running datastore.
    addrs = byname["r1-eth0"]["frr-zebra:zebra"]["ipv4-addrs"]
    assert any(a["ip"] == "192.0.2.1" for a in addrs)


def test_get_state_empty_string_path(tgen):
    r1 = tgen.gears["r1"]

    step("An explicit empty-string path reads the root like s018 did")
    check_backend_oper_served(r1)

    output = run_grpc_client(r1, "GET-STATE,")
    assert '"if-index"' in output


def test_get_state_multiple_paths(tgen):
    r1 = tgen.gears["r1"]

    step("One Get(STATE) request with two paths streams one response per path")
    check_backend_oper_served(r1)

    output = run_grpc_client(r1, "GET-STATE-PATHS,/frr-vrf:lib;/frr-interface:lib")
    responses = output.split("===RESPONSE===")
    assert len(responses) == 2
    assert '"if-index"' in output
    assert "frr-vrf:lib" in output


def test_get_state_mgmtd_local_backend(tgen):
    r1 = tgen.gears["r1"]

    step("mgmtd-local oper state flows through the in-process backend client")
    output = run_grpc_client(r1, "GET-STATE,/frr-backend:clients")
    out_json = json.loads(output)
    clients = out_json["frr-backend:clients"]["client"]
    names = {c["name"] for c in clients}
    assert "zebra" in names


def _run_until_sync(r, xpath, timeout=15):
    cmd = f"SUBSCRIBE-UNTIL-SYNC,{xpath},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_sample_count(r, xpath, interval_ms=200, count=3, timeout=5):
    cmd = f"SUBSCRIBE-SAMPLE-COUNT,{xpath},{interval_ms},{count},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_sample_count_typed(
    r, xpath, snapshot_type, interval_ms=200, count=3, timeout=5
):
    cmd = (
        "SUBSCRIBE-SAMPLE-COUNT-TYPED,"
        f"{xpath},{interval_ms},{count},{snapshot_type},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def test_subscribe_stream_backend_state(tgen):
    r1 = tgen.gears["r1"]

    step("A STREAM initial snapshot serves zebra-owned interface state")
    check_backend_oper_served(r1)

    raw = _run_until_sync(r1, "/frr-interface:lib", timeout=15).strip()
    responses = json.loads(raw.splitlines()[-1])
    assert responses, "STREAM Subscribe returned no responses"
    assert responses[-1] == {"sync_response": True}
    updates = [item for item in responses if "update" in item]
    assert updates, "STREAM Subscribe returned no initial snapshot"
    assert all(update["path"] == "/frr-interface:lib" for update in updates)
    assert any('"if-index"' in update["update"] for update in updates)


def test_subscribe_sample_backend_state(tgen):
    r1 = tgen.gears["r1"]

    step("SAMPLE snapshots serve zebra-owned interface state periodically")
    check_backend_oper_served(r1)

    raw = _run_sample_count(
        r1, "/frr-interface:lib", interval_ms=200, count=3, timeout=5
    ).strip()
    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 3
    assert all('"if-index"' in response["data"] for response in responses)


def test_subscribe_sample_all_merges_config_and_state(tgen):
    r1 = tgen.gears["r1"]

    step("SAMPLE ALL snapshots merge running config with backend state")
    check_backend_oper_served(r1)

    raw = _run_sample_count_typed(r1, "/frr-interface:lib", "ALL").strip()
    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 3
    for response in responses:
        byname = interfaces_by_name(response["data"])
        r1_eth0 = byname["r1-eth0"]
        assert "if-index" in r1_eth0["state"]
        addrs = r1_eth0["frr-zebra:zebra"]["ipv4-addrs"]
        assert any(a["ip"] == "192.0.2.1" for a in addrs)
