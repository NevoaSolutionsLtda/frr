# SPDX-License-Identifier: ISC
"""
Verifies the Fase C fatia 3 northbound wiring in bgpd: the bmp
monitor l2vpn-evpn leaves under
global/bmp-config/target-list/afi-safis/afi-safi/l2vpn-evpn/
common-config/{pre,post-policy,loc-rib} -- become programmable
through the mgmtd gRPC bridge.

RED on the s059 head (afe44d26e): the commits hit the reject-strict
stubs and fail. GREEN on the s060 head: the commits apply through
bmp_monitor_apply(), the same internal the "bmp monitor" DEFUN calls,
and the legacy CLI surface (show running-config) renders the exact
"bmp monitor l2vpn evpn <policy>" lines, proving the datastore and
the bgpd internals agree.

The bmp target group is seeded with the legacy CLI first: its
target-list create remains a warn-class stub (Fase D scope), the same
NB_CLIENT_CLI seeding rationale as unnumbered neighbors. A commit
against a group that does not exist in bgpd must fail the commit with
an explicit error (no silent no-op). The CLI dual-write path keeps
working after the bmp_monitor_cfg refactor (regression guard).

The bmp monitor internals live in the bgpd_bmp loadable module: the
core callbacks reach them through the bgp_nb_bmp_ops bridge the
module fills at load time (bgpd starts with -M bgpd_bmp here).
"""
import glob
import os
import sys

import pytest
from lib.common_config import step
from lib.topogen import Topogen, TopoRouter, get_topogen

CWD = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CWD, "../"))

GRPCP_MGMTD = 50071
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.bgpd, pytest.mark.mgmtd]

CPP = (
    "/frr-routing:routing/control-plane-protocols/control-plane-protocol"
    "[type='frr-bgp:bgp'][name='bgp'][vrf='default']/frr-bgp:bgp"
)

TARGET = "bt-grpc"
TARGET_CLI = "bt-cli"
TARGET_MISSING = "bt-nobody"


def bmp_common(target):
    return (
        f"{CPP}/global/bmp-config/target-list[target-name='{target}']"
        "/afi-safis/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']"
        "/l2vpn-evpn/common-config"
    )


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


def _bgp_bmp_module_available():
    """True when the bgpd_bmp loadable module is installed (the bmp
    monitor internals live behind it; the core callbacks fail with
    'module not loaded' without it)."""
    patterns = (
        "/usr/lib/*/frr/modules/bgpd_bmp.so",
        "/usr/lib/frr/modules/bgpd_bmp.so",
        "/usr/lib64/*/frr/modules/bgpd_bmp.so",
        "/usr/lib64/frr/modules/bgpd_bmp.so",
        "/usr/local/lib/*/frr/modules/bgpd_bmp.so",
        "/usr/local/lib/frr/modules/bgpd_bmp.so",
    )
    for pattern in patterns:
        for path in glob.glob(pattern):
            if os.path.isfile(path):
                return True
    return False


if not _bgp_bmp_module_available():
    pytest.skip(
        "skipping; bgpd_bmp module not installed",
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


def build_topo(tgen):
    tgen.add_router("r1")
    switch = tgen.add_switch("s1")
    switch.add_link(tgen.gears["r1"])


def setup_module(mod):
    tgen = Topogen(build_topo, mod.__name__)
    tgen.start_topology()
    router = tgen.gears["r1"]
    router.load_config(
        "bgpd", os.path.join(CWD, "r1/bgpd.conf"), "-M bgpd_bmp"
    )
    router.load_config(TopoRouter.RD_MGMTD, "", f"-M grpc:{GRPCP_MGMTD}")
    tgen.start_router()


def teardown_module():
    tgen = get_topogen()
    tgen.stop_topology()


def run_grpc_client(r, commands):
    if not isinstance(commands, str):
        commands = "\n".join(commands) + "\n"
    if not commands.endswith("\n"):
        commands += "\n"
    return r.cmd_raises(
        [script_path, f"--port={GRPCP_MGMTD}"], stdin=commands
    )


def run_grpc_client_status(r, command):
    if not command.endswith("\n"):
        command += "\n"
    return r.net.cmd_status(
        [script_path, f"--port={GRPCP_MGMTD}"], stdin=command
    )


@pytest.fixture(autouse=True)
def skip_on_failure():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)


def _seed(r1):
    """Idempotent: seed local-as into the datastore (mandatory under
    global; mgmtd's copy does not track CLI-written config --
    NB_CLIENT_CLI exemption). Status variant: a no-op re-seed is
    fine."""
    run_grpc_client_status(r1, f"commit-set,{CPP}/global/local-as=65000")


def _seed_target(r1, name):
    """Idempotent: create the bmp target group with the legacy CLI
    (target-list create stays a warn-class stub until Fase D; mgmtd's
    copy does not track CLI-written config -- NB_CLIENT_CLI
    exemption)."""
    r1.vtysh_cmd(
        "configure terminal\n"
        "router bgp 65000\n"
        f"bmp targets {name}\n"
        "end\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"bmp targets {name}" in output, (
        f"seed failed for {name}:\n{output}"
    )


def test_bmp_monitor_grpc_all_three_policies():
    """pre-policy, post-policy and loc-rib land through gRPC and render
    as the exact legacy CLI lines; commit-delete returns each leaf to
    the default."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _seed_target(r1, TARGET)
    bc = bmp_common(TARGET)

    step("Set the three monitor policies via gRPC")
    run_grpc_client(
        r1,
        [
            f"commit-set,{bc}/pre-policy=true",
            f"commit-set,{bc}/post-policy=true",
            f"commit-set,{bc}/loc-rib=true",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor l2vpn evpn pre-policy" in output, (
        f"pre-policy missing:\n{output}"
    )
    assert "bmp monitor l2vpn evpn post-policy" in output, (
        f"post-policy missing:\n{output}"
    )
    assert "bmp monitor l2vpn evpn loc-rib" in output, (
        f"loc-rib missing:\n{output}"
    )

    step("Destroy each leaf one by one")
    for leaf in ("pre-policy", "post-policy", "loc-rib"):
        run_grpc_client(r1, f"commit-delete,{bc}/{leaf}")
        output = r1.vtysh_cmd("show running-config bgpd")
        assert f"bmp monitor l2vpn evpn {leaf}" not in output, (
            f"{leaf} must be gone:\n{output}"
        )
    assert "bmp monitor" not in output, (
        f"all monitor lines must be gone:\n{output}"
    )


def test_bmp_monitor_reshape_and_reapply():
    """reshape: delete only pre-policy while post-policy survives, then
    re-apply pre-policy; an idempotent re-commit of the same value is
    rejected as a no-op (mgmtd no-changes abort) and the running state
    survives unchanged."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _seed_target(r1, TARGET)
    bc = bmp_common(TARGET)

    step("pre + post set; pre deleted, post survives")
    run_grpc_client(
        r1,
        [
            f"commit-set,{bc}/pre-policy=true",
            f"commit-set,{bc}/post-policy=true",
        ],
    )
    run_grpc_client(r1, f"commit-delete,{bc}/pre-policy")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor l2vpn evpn pre-policy" not in output, (
        f"pre-policy must be gone:\n{output}"
    )
    assert "bmp monitor l2vpn evpn post-policy" in output, (
        f"post-policy must survive:\n{output}"
    )

    step("re-apply pre-policy; idempotent re-commit rejected as no-op")
    run_grpc_client(r1, f"commit-set,{bc}/pre-policy=true")
    rc, output, _ = run_grpc_client_status(
        r1, f"commit-set,{bc}/pre-policy=true"
    )
    # mgmtd quirk (documented in the fanout suite): re-committing the
    # same value aborts with "No changes found to be committed" --
    # rc != 0 with empty output. What must hold: the running state
    # survives the rejected no-op unchanged (asserted below).
    assert rc != 0, (
        f"idempotent re-commit must surface the no-changes abort; "
        f"rc={rc} output={output!r}"
    )
    output_run = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor l2vpn evpn pre-policy" in output_run, (
        f"pre-policy must be back:\n{output_run}"
    )
    assert "bmp monitor l2vpn evpn post-policy" in output_run

    step("teardown both")
    run_grpc_client(
        r1,
        [
            f"commit-delete,{bc}/pre-policy",
            f"commit-delete,{bc}/post-policy",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor" not in output, f"teardown sujou:\n{output}"


def test_bmp_monitor_missing_target_rejected():
    """a commit against a target group that does not exist in bgpd
    fails the commit with an explicit error (no silent no-op)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    bc = bmp_common(TARGET_MISSING)
    rc, output, err = run_grpc_client_status(
        r1, f"commit-set,{bc}/pre-policy=true"
    )
    # The read-only checks run at VALIDATE, so the refusal arrives as
    # a gRPC status error whose details name the target (contract
    # nuance 4.1: VALIDATE errors are status errors, not in-band)
    assert rc != 0, (
        f"commit to missing target must fail; rc={rc} output={output!r}"
    )
    assert "not found" in (err or "").lower() and TARGET_MISSING in (
        err or ""
    ), (
        f"error must name the missing target:\n{err}"
    )


def test_bmp_monitor_cli_parity_after_refactor():
    """the bmp_monitor_cfg CLI keeps working through the shared
    bmp_monitor_apply() internal: set and unset via CLI render exactly
    as before the refactor (dual-write regression guard)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("CLI: create target + set loc-rib")
    r1.vtysh_cmd(
        "configure terminal\n"
        "router bgp 65000\n"
        f"bmp targets {TARGET_CLI}\n"
        "bmp monitor l2vpn evpn loc-rib\n"
        "end\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor l2vpn evpn loc-rib" in output, (
        f"CLI loc-rib missing:\n{output}"
    )

    step("CLI: unset loc-rib")
    r1.vtysh_cmd(
        "configure terminal\n"
        "router bgp 65000\n"
        f"bmp targets {TARGET_CLI}\n"
        "no bmp monitor l2vpn evpn loc-rib\n"
        "end\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bmp monitor l2vpn evpn loc-rib" not in output, (
        f"CLI loc-rib must be gone:\n{output}"
    )
