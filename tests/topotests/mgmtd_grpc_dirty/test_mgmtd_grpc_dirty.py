# SPDX-License-Identifier: ISC
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
#
# Copyright (C) 2026  Reinaldo Saraiva
#

"""
Test the gRPC commit dirty-candidate refusal (org issue #11).

A native frontend session can stage changes in the shared candidate
datastore without committing them.  A gRPC commit (or validate) must
refuse to install its request-local candidate over that staging instead
of clobbering it, and the staged work must still reach running when the
native session commits.
"""

import glob
import json
import os

import pytest
from lib.common_config import step
from lib.topogen import Topogen, TopoRouter

CWD = os.path.dirname(os.path.realpath(__file__))
GRPCP_MGMTD = 50064
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.mgmtd]

DESC_XPATH = "/frr-interface:lib/interface[name='r1-eth0']/description"
NATIVE_VALUE = "s054-native-staged"
GRPC_VALUE = "s054-grpc-refused"


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


def _candidate_json(r):
    return r.vtysh_cmd("show mgmt datastore-contents candidate json")


def test_commit_refused_over_native_staging(tgen):
    r1 = tgen.gears["r1"]

    step("Stage native config without committing it")
    r1.vtysh_cmd(
        f"configure terminal file-lock\nmgmt set-config {DESC_XPATH} {NATIVE_VALUE}"
    )
    assert NATIVE_VALUE in _candidate_json(r1)

    step("A gRPC commit over the staged candidate is refused")
    output = run_grpc_client(r1, f"commit-result,ALL,{DESC_XPATH}={GRPC_VALUE}")
    result = json.loads(output.strip().splitlines()[-1])
    assert result["status"] != "OK"
    assert "staged" in result.get("details", "")

    step("A gRPC VALIDATE over the staged candidate is refused too")
    output = run_grpc_client(r1, f"commit-result,VALIDATE,{DESC_XPATH}={GRPC_VALUE}")
    result = json.loads(output.strip().splitlines()[-1])
    assert result["status"] != "OK"
    assert "staged" in result.get("details", "")

    step("The native staging survived untouched")
    assert NATIVE_VALUE in _candidate_json(r1)
    assert GRPC_VALUE not in _candidate_json(r1)

    step("The native session can still commit its staging")
    r1.vtysh_cmd("configure terminal file-lock\nmgmt commit apply")
    running = r1.vtysh_cmd("show running-config")
    assert NATIVE_VALUE in running
    assert GRPC_VALUE not in running

    step("Clean up the staged description")
    run_grpc_client(r1, f"commit-delete,{DESC_XPATH}")
