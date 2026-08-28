# SPDX-License-Identifier: ISC
"""
Verifies the Fase B fatia 1 northbound wiring in bgpd: the prefix-limit
subtree (peer_maximum_prefix_* internals) and the network-config
subtree (bgp_static_* internals) become programmable through the mgmtd
gRPC bridge.

RED on the base trunk (S056 head f97d994cc): the commits below hit
reject-strict stubs and fail. GREEN on the s057 head: the commits apply
and the legacy CLI surface (show running-config) renders the exact knob
lines, proving the datastore and the bgpd internals agree.

Fase C fatia 1 wired the l2vpn-evpn prefix-limit fanout (shared
callbacks); the flipped test guards that the commit now applies.

Fase D flipped the stub policy to default-reject: a programmatic write
on a knob that is neither wired nor allowlisted must fail validation
with the reject-strict error, while the legacy CLI keeps its exemption.
"""
import glob
import json
import os
import sys

import pytest
from lib.common_config import step
from lib.topogen import Topogen, TopoRouter, get_topogen

CWD = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CWD, "../"))

GRPCP_MGMTD = 50065
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.bgpd, pytest.mark.mgmtd]

CPP = (
    "/frr-routing:routing/control-plane-protocols/control-plane-protocol"
    "[type='frr-bgp:bgp'][name='bgp'][vrf='default']/frr-bgp:bgp"
)
PEER = "10.0.0.2"
NB = f"{CPP}/neighbors/neighbor[remote-address='{PEER}']"
AF = (f"{NB}/afi-safis"
      "/afi-safi[afi-safi-name='frr-routing:ipv4-unicast']/ipv4-unicast")
AF_G = (f"{CPP}/global/afi-safis"
        "/afi-safi[afi-safi-name='frr-routing:ipv4-unicast']"
        "/ipv4-unicast")
AF_VPN = (f"{CPP}/global/afi-safis"
          "/afi-safi[afi-safi-name='frr-routing:l3vpn-ipv4-unicast']"
          "/l3vpn-ipv4-unicast")
EVPN_AF = (
    f"{NB}/afi-safis"
    "/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"
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
    router.load_config("bgpd", os.path.join(CWD, "r1/bgpd.conf"))
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


def test_prefix_limit_inbound_grpc():
    """G-PL: maximum-prefix + threshold land in the peer internals and
    render back on the legacy CLI."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the neighbor context, then the prefix-limit (one shot)")
    run_grpc_client(
        r1,
        [
            f"commit-set,{CPP}/global/local-as=65000",
            # one transaction: remote-as-type is a mandatory leaf and
            # the wired validation demands remote-as in the same txn
            f"commit-result,ALL,"
            f"{NB}/neighbor-remote-as/remote-as-type=as-specified,"
            f"{NB}/neighbor-remote-as/remote-as=65001,"
            f"{AF}/prefix-limit/direction-list"
            "[direction='in']/max-prefixes=2,"
            f"{AF}/prefix-limit/direction-list"
            "[direction='in']/options/shutdown-threshold-pct=80",
        ]
    )

    step("The legacy CLI shows the maximum-prefix line")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix 2 80" in output, (
        f"expected maximum-prefix on legacy CLI; got:\n{output}"
    )

    step("The gRPC get-config view agrees (round-trip)")
    out = run_grpc_client(r1, f"get-config,{AF}/prefix-limit")
    assert "2" in out and "80" in out, f"prefix-limit missing: {out}"


def test_prefix_limit_outbound_grpc():
    """G-PL: direction=out maps onto maximum-prefix-out."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    run_grpc_client(
        r1,
        f"commit-set,{AF}/prefix-limit/direction-list[direction='out']"
        "/max-prefixes=5",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix-out 5" in output, (
        f"expected maximum-prefix-out on legacy CLI; got:\n{output}"
    )


def test_prefix_limit_destroy_grpc():
    """G-PL: destroying the direction-list unsets the internals."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    run_grpc_client(
        r1,
        f"commit-delete,{AF}/prefix-limit/direction-list[direction='out']",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix-out" not in output, (
        f"maximum-prefix-out should be gone; got:\n{output}"
    )
    assert "maximum-prefix 2 80" in output, "inbound half must survive"


def test_network_config_grpc():
    """G-NC: network-config lands in bgp static routes and the route is
    originated (the link subnet satisfies import-check)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Announce the r1-eth0 link subnet")
    # (the grpc-query CLI is path=value only; touching the defaulted
    # backdoor leaf expresses the list-entry CREATE)
    run_grpc_client(
        r1,
        f"commit-set,{AF_G}/network-config[prefix='10.0.0.0/24']"
        "/backdoor=false",
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 10.0.0.0/24" in output, (
        f"expected network on legacy CLI; got:\n{output}"
    )

    step("The static route is originated into the BGP table")
    output = r1.vtysh_cmd("show bgp ipv4 unicast 10.0.0.0/24 json")
    assert "10.0.0.0/24" in output, (
        f"expected the network in the BGP table; got:\n{output}"
    )


def test_network_config_backdoor_grpc():
    """G-NC: the backdoor leaf round-trips and the entry is removable."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    run_grpc_client(
        r1,
        f"commit-set,{AF_G}/network-config[prefix='10.0.0.0/24']"
        "/backdoor=true",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 10.0.0.0/24 backdoor" in output, (
        f"expected backdoor on legacy CLI; got:\n{output}"
    )

    run_grpc_client(
        r1, f"commit-delete,{AF_G}/network-config[prefix='10.0.0.0/24']"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 10.0.0.0/24" not in output, (
        f"network should be gone; got:\n{output}"
    )


def test_prefix_limit_option_destroy_grpc():
    """G-PL: destroying an individual option leaf unsets the knob."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Re-state the base + threshold in one txn (runs after the neighbor test)")
    run_grpc_client(
        r1,
        f"commit-result,ALL,"
        f"{AF}/prefix-limit/direction-list[direction='in']"
        "/max-prefixes=2,"
        f"{AF}/prefix-limit/direction-list[direction='in']"
        "/options/shutdown-threshold-pct=50",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix 2 50" in output, (
        f"expected explicit threshold; got:\n{output}"
    )

    step("Destroy the option leaf; the explicit threshold is gone")
    run_grpc_client(
        r1,
        f"commit-delete,{AF}/prefix-limit/direction-list[direction='in']"
        "/options/shutdown-threshold-pct",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix 2 50" not in output, (
        f"explicit threshold should be gone; got:\n{output}"
    )
    assert "maximum-prefix 2" in output, "inbound limit must survive"


def test_network_config_l3vpn_grpc():
    """G-NC l3vpn: rd/prefix-list entries with label-index and
    route-map (the leaf modify/destroy paths under network-config[rd])."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the rd + prefix-list entry via gRPC")
    run_grpc_client(
        r1,
        f"commit-set,{AF_VPN}/network-config[rd='65000:100']"
        "/prefix-list[prefix='198.18.9.0/24']/label-index=1000",
    )

    step("label-index modify on the existing entry (depth-4 leaf path)")
    # the legacy internals refuse a label CHANGE (parity); the modify
    # must fail cleanly AND flow through the leaf callback without
    # aborting the daemon -- that is the depth regression under test
    rc, stdout, stderr = run_grpc_client_status(
        r1,
        f"commit-set,{AF_VPN}/network-config[rd='65000:100']"
        "/prefix-list[prefix='198.18.9.0/24']/label-index=1001",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "198.18.9.0/24" in output, (
        f"bgpd lost the vpn network after a leaf modify; got:\n{output}"
    )

    step("rmap-policy-export set and unset on the entry")
    run_grpc_client(
        r1,
        f"commit-set,{AF_VPN}/network-config[rd='65000:100']"
        "/prefix-list[prefix='198.18.9.0/24']/rmap-policy-export=s057rm",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "198.18.9.0/24 rd 65000:100 label 1000 route-map s057rm" in output, (
        f"expected vpn network line; got:\n{output}"
    )
    run_grpc_client(
        r1,
        f"commit-delete,{AF_VPN}/network-config[rd='65000:100']"
        "/prefix-list[prefix='198.18.9.0/24']/rmap-policy-export",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "route-map s057rm" not in output, "rmap should be unset"

    step("destroy the rd entry (children included)")
    run_grpc_client(
        r1,
        f"commit-delete,{AF_VPN}/network-config[rd='65000:100']",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "198.18.9.0/24" not in output, (
        f"vpn network should be gone; got:\n{output}"
    )


def test_prefix_limit_multi_leaf_option_destroy():
    """G-PL: destroying ONE leaf of the tr case keeps the sibling alive
    (review round 2, A1) and half-cases are rejected (A2)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("State the full tr case in one txn")
    run_grpc_client(
        r1,
        f"commit-result,ALL,"
        f"{AF}/prefix-limit/direction-list[direction='in']"
        "/max-prefixes=3,"
        f"{AF}/prefix-limit/direction-list[direction='in']"
        "/options/tr-shutdown-threshold-pct=40,"
        f"{AF}/prefix-limit/direction-list[direction='in']"
        "/options/tr-restart-timer=100",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix 3 40 restart 100" in output, (
        f"expected full tr case; got:\n{output}"
    )

    step("Destroy only the threshold; the timer must survive")
    run_grpc_client(
        r1,
        f"commit-delete,{AF}/prefix-limit/direction-list[direction='in']"
        "/options/tr-shutdown-threshold-pct",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "restart 100" in output, (
        f"tr-restart-timer must survive its sibling destroy; got:\n{output}"
    )
    assert "3 40" not in output, "threshold should be reset"

    step("A half-case commit is rejected (tr timer without threshold)")
    rc, stdout, stderr = run_grpc_client_status(
        r1,
        f"commit-set,{AF}/prefix-limit/direction-list[direction='in']"
        "/options/tr-restart-timer=77",
    )
    assert rc != 0 or "requires" in (stdout + stderr), (
        f"orphan tr-restart-timer should be refused; got: {stdout+stderr}"
    )


def test_network_config_cli_authority():
    """G-NC CLI authority: a bare `network X` (no knobs) configures the
    internals, and a re-issue WITHOUT backdoor mirrors the legacy knob
    clear (review rounds 1-2, B2/A3). (The CLI dual mirrors into bgpd's own
    northbound datastore; mgmtd's copy only tracks mgmtd-fronted
    commits -- the NB_CLIENT_CLI exemption design.)"""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Announce a bare network through the legacy CLI")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        "network 198.18.77.0/24\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 198.18.77.0/24" in output, (
        f"legacy CLI missing the bare network; got:\n{output}"
    )

    step("Set backdoor, then re-issue bare: the knob must clear")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        "network 198.18.77.0/24 backdoor\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 198.18.77.0/24 backdoor" in output, (
        f"backdoor should be set; got:\n{output}"
    )
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        "network 198.18.77.0/24\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 198.18.77.0/24\n" in output, (
        f"re-issue must render the bare form (knob cleared); got:\n{output}"
    )

    step("Set route-map, then re-issue bare: the rmap must clear")
    r1.vtysh_cmd(
        "configure terminal\nroute-map s057rm permit 1\nexit\n"
        "router bgp 65000\n"
        "address-family ipv4 unicast\n"
        "network 198.18.77.0/24 route-map s057rm\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 198.18.77.0/24 route-map s057rm" in output, (
        f"rmap should be set; got:\n{output}"
    )
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        "network 198.18.77.0/24\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "network 198.18.77.0/24 route-map" not in output, (
        f"re-issue must clear the network rmap; got:\n{output}"
    )

    step("Destroy removes the entry from the internals")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        "no network 198.18.77.0/24\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "198.18.77.0/24" not in output, "destroy left the network"


def test_evpn_prefix_limit_wired():
    """Fase C fatia 1 flipped the l2vpn-evpn prefix-limit from the
    reject-strict stub class to the shared wired callbacks from
    Fase B: the gRPC commit now applies and renders on the legacy
    CLI."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("l2vpn-evpn prefix-limit commit must apply")
    run_grpc_client(
        r1,
        [
            f"commit-set,{NB}/afi-safis/afi-safi"
            "[afi-safi-name='frr-routing:l2vpn-evpn']/enabled=true",
            f"commit-set,{EVPN_AF}/prefix-limit/direction-list"
            "[direction='in']/max-prefixes=2",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix 2" in output, (
        f"expected EVPN maximum-prefix on legacy CLI; got:\n{output}"
    )

    step("Destroying the direction-list withdraws it")
    run_grpc_client(
        r1,
        f"commit-delete,{EVPN_AF}/prefix-limit/direction-list"
        "[direction='in']",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix 2\n" not in output, (
        f"EVPN maximum-prefix must be gone; got:\n{output}"
    )


def test_cli_write_keeps_authority_and_datastore_intact():
    """The CLI keeps legacy authority: a legacy `neighbor maximum-prefix`
    applies to the bgpd internals (legacy show reflects it) without
    corrupting the gRPC-visible datastore view. (The CLI dual-write
    mirrors into the daemon-side datastore -- mgmtd's copy only
    tracks mgmtd-fronted commits, per the NB_CLIENT_CLI exemption
    design in bgp_nb_stubs.c.)"""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Re-configure maximum-prefix through the legacy CLI")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family ipv4 unicast\n"
        f"neighbor {PEER} maximum-prefix 100\n"
    )

    step("The legacy CLI is still the authority on the internals")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix 100" in output, (
        f"expected CLI-configured maximum-prefix; got:\n{output}"
    )

    step("The gRPC datastore view stays queryable (no corruption)")
    out = run_grpc_client(r1, f"get-config,{AF}/prefix-limit")
    assert "prefix-limit" in out or "direction-list" in out, (
        f"datastore view lost the prefix-limit subtree: {out}"
    )


def _json_find_value(obj, key_sub, val):
    """True when any json key containing key_sub holds val (recursive)."""
    if isinstance(obj, dict):
        for k, v in obj.items():
            if key_sub in k.lower() and str(v) == str(val):
                return True
            if _json_find_value(v, key_sub, val):
                return True
    elif isinstance(obj, list):
        for it in obj:
            if _json_find_value(it, key_sub, val):
                return True
    return False


def test_neighbor_timers_grpc():
    """S061: the session timers pair lands in the peer internals and
    renders back on the legacy CLI (numbered context)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Set keepalive=3 hold-time=9 in one transaction")
    run_grpc_client(
        r1,
        [
            f"commit-set,{NB}/timers/keepalive=3",
            f"commit-set,{NB}/timers/hold-time=9",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert output.count("neighbor 10.0.0.2 timers 3 9") == 1, (
        f"expected exactly one timers line on legacy CLI; got:\n{output}"
    )

    step("The peer internals carry the negotiated timers")
    j = json.loads(r1.vtysh_cmd("show bgp neighbors 10.0.0.2 json"))
    assert _json_find_value(j, "configuredkeepalive", 3000), (
        f"configured keepalive 3s not found in neighbor json: {j}"
    )
    assert _json_find_value(j, "configuredhold", 9000), (
        f"configured hold-time 9s not found in neighbor json: {j}"
    )


def test_neighbor_timers_single_leaf_grpc():
    """S061: a single leaf still applies (regression: the old
    container apply_finish required BOTH leaves and silently
    no-oped otherwise)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Clear the pair, then set ONLY hold-time")
    run_grpc_client(
        r1,
        [
            f"commit-delete,{NB}/timers/keepalive",
            f"commit-delete,{NB}/timers/hold-time",
        ],
    )
    run_grpc_client(r1, f"commit-set,{NB}/timers/hold-time=15")
    output = r1.vtysh_cmd("show running-config bgpd")
    # keepalive defaults to 60 in the datastore but the legacy
    # internals clamp it to holdtime/3 (peer_timers_set), and the
    # legacy render prints the internals
    assert output.count("neighbor 10.0.0.2 timers 5 15") == 1, (
        f"hold-time alone must render once with the clamped keepalive; "
        f"got:\n{output}"
    )
    j = json.loads(r1.vtysh_cmd("show bgp neighbors 10.0.0.2 json"))
    assert _json_find_value(j, "configuredhold", 15000), (
        f"configured hold-time 15s not found in neighbor json: {j}"
    )

    step("Setting keepalive afterwards keeps the sibling")
    run_grpc_client(r1, f"commit-set,{NB}/timers/keepalive=5")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert output.count("neighbor 10.0.0.2 timers 5 15") == 1, (
        f"keepalive modify must keep hold-time exactly once; got:\n{output}"
    )


def test_neighbor_timers_destroy_grpc():
    """S061: destroying one leaf reverts it to default; destroying the
    last leaf removes the whole timers line."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Destroy keepalive; hold-time survives with the clamped default K")
    run_grpc_client(r1, f"commit-delete,{NB}/timers/keepalive")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert output.count("neighbor 10.0.0.2 timers 5 15") == 1, (
        f"keepalive destroy must revert to the clamped default once; "
        f"got:\n{output}"
    )

    step("Destroy hold-time; the timers line is gone")
    run_grpc_client(r1, f"commit-delete,{NB}/timers/hold-time")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 timers " not in output, (
        f"timers line should be gone; got:\n{output}"
    )


def test_peer_group_timers_grpc():
    """S061: the peer-group context shares the callbacks; the timers
    render under the group name and fan out to members through the
    legacy peer_timers_set internals."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the peer-group context with timers in one txn")
    pg = f"{CPP}/peer-groups/peer-group[peer-group-name='s061pg']"
    run_grpc_client(
        r1,
        [
            f"commit-set,{pg}/neighbor-remote-as/remote-as-type=as-specified",
            f"commit-set,{pg}/neighbor-remote-as/remote-as=65001",
            f"commit-set,{pg}/timers/keepalive=7",
            f"commit-set,{pg}/timers/hold-time=21",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert output.count("neighbor s061pg timers 7 21") == 1, (
        f"expected exactly one peer-group timers line; got:\n{output}"
    )

    step("Destroy the pair; the group timers line is gone")
    run_grpc_client(
        r1,
        [
            f"commit-delete,{pg}/timers/keepalive",
            f"commit-delete,{pg}/timers/hold-time",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor s061pg timers " not in output, (
        f"peer-group timers should be gone; got:\n{output}"
    )


UNWIRED_KNOB = CPP + "/global/graceful-restart/disable-eor"
DAEMON_KNOB = "/frr-bgp:bgp-daemon/session-dscp"


def test_daemon_subtree_rejected_grpc():
    """Fase D follow-up: the daemon-wide /frr-bgp:bgp-daemon subtree is
    now in bgpd_config_xpaths, so the reject-strict class can actually
    fire there. Before the subscription a programmatic write on the
    subtree never reached bgpd: mgmtd committed it datastore-only and
    the client saw a false commit-OK (silent no-op)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Ensure the router base exists so a refusal cannot hide behind"
         " the mandatory local-as check")
    run_grpc_client_status(r1, f"commit-set,{CPP}/global/local-as=65000")

    step("A gRPC write on the bgp-daemon subtree is refused"
         " (reject-strict)")
    rc, stdout, stderr = run_grpc_client_status(
        r1, f"commit-set,{DAEMON_KNOB}=10"
    )
    assert "reject-strict" in (stdout + stderr), (
        f"bgp-daemon write must fail with reject-strict now that bgpd"
        f" subscribes the prefix; got: {stdout + stderr}"
    )

    step("The legacy CLI still owns the knob (CLI is exempt)")
    r1.vtysh_cmd("configure terminal\nbgp session-dscp 10\n")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bgp session-dscp 10" in output, (
        f"CLI write must keep authority over the daemon knob; got:\n"
        f"{output}"
    )

    step("A gRPC re-assertion of the CLI-applied knob is refused too")
    rc, stdout, stderr = run_grpc_client_status(
        r1, f"commit-set,{DAEMON_KNOB}=10"
    )
    assert "reject-strict" in (stdout + stderr), (
        f"programmatic re-assertion of the CLI-applied daemon knob must"
        f" fail with reject-strict; got: {stdout + stderr}"
    )

    step("The refusals leave the commit path healthy (wired knob)")
    # 250 (not 200) so the health check of test_unwired_write_rejected
    # -- which re-asserts local-pref=200 -- never degenerates into an
    # empty "No changes found" commit when both tests run in sequence
    run_grpc_client(r1, f"commit-set,{CPP}/global/local-pref=250")
    out = run_grpc_client(r1, f"get-config,{CPP}/global/local-pref")
    assert "250" in out, (
        f"wired commit after the refusals must still apply; got: {out}"
    )


def test_unwired_write_rejected_grpc():
    """Fase D: the default-reject policy. A knob that is neither wired
    nor allowlisted (global/graceful-restart/disable-eor) refuses
    programmatic writes with the reject-strict error -- including an
    re-assertion attempt after the legacy CLI has applied the knob --
    while the CLI dual-write keeps its NB_CLIENT_CLI exemption and
    later wired commits stay unaffected.

    The knob must live under a prefix bgpd subscribes to through
    mgmt_be (bgpd_config_xpaths): the /frr-bgp:bgp-daemon twin is NOT
    in that list, so commits there stay datastore-only and no stub
    class can fire."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Ensure the router base exists so a refusal cannot hide behind"
         " the mandatory local-as check")
    # best-effort: re-stating local-as when it is already set is an
    # empty commit mgmtd refuses (ABORTED); only the steps below are
    # under assert
    run_grpc_client_status(r1, f"commit-set,{CPP}/global/local-as=65000")

    step("A gRPC write on the unwired knob is refused (reject-strict)")
    rc, stdout, stderr = run_grpc_client_status(
        r1, f"commit-set,{UNWIRED_KNOB}=true"
    )
    assert "reject-strict" in (stdout + stderr), (
        f"unwired write must fail with reject-strict; got: "
        f"{stdout + stderr}"
    )

    step("The legacy CLI still owns the knob (CLI is exempt)")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "bgp graceful-restart disable-eor\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bgp graceful-restart disable-eor" in output, (
        f"CLI write must keep authority over the knob; got:\n{output}"
    )

    step("A gRPC re-assertion of the CLI-applied knob is refused too")
    rc, stdout, stderr = run_grpc_client_status(
        r1, f"commit-set,{UNWIRED_KNOB}=true"
    )
    assert "reject-strict" in (stdout + stderr), (
        f"programmatic re-assertion of the CLI-applied knob must fail "
        f"with reject-strict; got: {stdout + stderr}"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bgp graceful-restart disable-eor" in output, (
        f"the refused overwrite must leave the CLI knob intact; "
        f"got:\n{output}"
    )

    step("The refusals leave the commit path healthy (wired knob)")
    run_grpc_client(r1, f"commit-set,{CPP}/global/local-pref=200")
    out = run_grpc_client(r1, f"get-config,{CPP}/global/local-pref")
    assert "200" in out, (
        f"wired commit after the refusals must still apply; got: {out}"
    )


PG_RA = "pg-ra"
NBPG_RA = f"{CPP}/peer-groups/peer-group[peer-group-name='{PG_RA}']"
PG_MEMBER = "10.0.0.9"


def test_peer_group_remote_as_grpc():
    """The peer-group remote-as pair is wired: a single programmatic
    transaction creates the group and sets its AS (peer_group_remote_as
    internals), the legacy CLI renders it, and a later gRPC AS change
    propagates to a CLI-created member -- the peer-group semantic that
    separates peer_group_remote_as from a bare peer_as_change on the
    group conf."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the peer-group and its remote-as in one transaction")
    # local-as rides along: the YANG makes it mandatory on the bgp
    # container, so a fresh mgmtd datastore (no CPP entry yet -- the
    # boot config went straight to bgpd) rejects the create without it.
    # Re-asserting it when the entry already exists is a no-op diff.
    run_grpc_client(
        r1,
        [
            f"commit-result,ALL,"
            f"{CPP}/global/local-as=65000,"
            f"{NBPG_RA}/neighbor-remote-as/remote-as-type=as-specified,"
            f"{NBPG_RA}/neighbor-remote-as/remote-as=65077",
        ]
    )

    step("The legacy CLI shows the group and its remote-as")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"neighbor {PG_RA} peer-group" in output, (
        f"peer-group missing on legacy CLI; got:\n{output}"
    )
    assert f"neighbor {PG_RA} remote-as 65077" in output, (
        f"group remote-as missing on legacy CLI; got:\n{output}"
    )

    step("Add a member through the legacy CLI (binds to the group AS)")
    r1.vtysh_cmd(
        f"configure terminal\nrouter bgp 65000\n"
        f"neighbor {PG_MEMBER} peer-group {PG_RA}\n"
        f"end\n"
    )

    step("Change the group AS over gRPC; the member must follow")
    run_grpc_client(
        r1, f"commit-set,{NBPG_RA}/neighbor-remote-as/remote-as=65078"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"neighbor {PG_RA} remote-as 65078" in output, (
        f"group remote-as change missing; got:\n{output}"
    )
    summary = json.loads(r1.vtysh_cmd("show bgp summary json"))
    peer = summary["ipv4Unicast"]["peers"][PG_MEMBER]
    assert peer["remoteAs"] == 65078, (
        f"group AS change did not propagate to the member; "
        f"got: {peer['remoteAs']}"
    )
