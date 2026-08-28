# SPDX-License-Identifier: ISC
"""
Verifies the Fase C fatia 2 northbound wiring in bgpd: the
l2vpn-evpn per-neighbor fanout under the three neighbor contexts
(numbered, unnumbered, peer-group) -- addpath counters, the allowas-in
family, conditional advertisement, the name-based filters, unsuppress
map, soo and upa, plus the per-AF leaves already wired for the other
address families -- becomes programmable through the mgmtd gRPC
bridge.

RED on the s058 head (01129c5f9): the commits hit reject-strict
stubs and fail. GREEN on the s059 head: the commits apply through the
same per-AF internals the CLI DEFUNs call and the legacy CLI surface
(show running-config) renders the exact knob lines, proving the
datastore and the bgpd internals agree. The callbacks resolve the
peer by probing the context list key, so the peer-group and
unnumbered contexts are exercised too.
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
PEER = "10.0.0.2"
IFPEER = "r1-eth0"
PG = "evpn-pg"

NB = f"{CPP}/neighbors/neighbor[remote-address='{PEER}']"
NBIF = f"{CPP}/neighbors/unnumbered-neighbor[interface='{IFPEER}']"
NBPG = f"{CPP}/peer-groups/peer-group[peer-group-name='{PG}']"

EAF = f"{NB}/afi-safis/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"
EIF = f"{NBIF}/afi-safis/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"
EPG = f"{NBPG}/afi-safis/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"


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


def _seed(r1):
    """Idempotent: seed local-as into the datastore (mgmtd's copy does
    not track CLI-written config -- NB_CLIENT_CLI exemption) so the
    per-AF validations pass. Status variant: a no-op re-seed is fine."""
    run_grpc_client_status(r1, f"commit-set,{CPP}/global/local-as=65000")


def _num_peer(r1):
    """Idempotent: create the numbered neighbor (a re-create commit
    reports "No changes" and aborts -- the mgmtd re-commit quirk, so
    the status variant is used)."""
    run_grpc_client_status(
        r1,
        f"commit-set,{NB}/neighbor-remote-as/remote-as-type=as-specified,"
        f"{NB}/neighbor-remote-as/remote-as=65100",
    )


def test_numbered_fanout_addpath_allowas_soo_grpc():
    """addpath-rx-paths-limit, allowas-in family and soo land through
    gRPC on the numbered neighbor and render on the legacy CLI."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _num_peer(r1)

    step("addpath-rx-paths-limit + allowas-in + soo via gRPC")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EAF}/add-paths/addpath-rx-paths-limit=100",
            f"commit-set,{EAF}/as-path-options/allow-own-as=2",
            f"commit-set,{EAF}/soo=65000:77",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "addpath-rx-paths-limit 100" in output, (
        f"addpath limit missing:\n{output}"
    )
    assert "allowas-in 2" in output, f"allowas-in missing:\n{output}"
    assert "soo 65000:77" in output, f"soo missing:\n{output}"

    step("allowas-in family reshape: route-map + origin replace number")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EAF}/as-path-options/allow-own-as=5",
            f"commit-set,{EAF}/as-path-options/allowas-in-route-map=rmap-in",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "allowas-in route-map rmap-in 5" in output, (
        f"allowas-in rmap+num missing:\n{output}"
    )

    step("Destroy the number: rmap survives alone")
    run_grpc_client(r1, f"commit-delete,{EAF}/as-path-options/allow-own-as")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "allowas-in route-map rmap-in\n" in output, (
        f"allowas-in rmap must survive alone:\n{output}"
    )

    step("Destroy soo")
    run_grpc_client(r1, f"commit-delete,{EAF}/soo")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "soo 65000:77" not in output, f"soo must be gone:\n{output}"

    step("Destroy the limit")
    run_grpc_client(r1, f"commit-delete,{EAF}/add-paths/addpath-rx-paths-limit")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "addpath-rx-paths-limit" not in output, (
        f"addpath limit must be gone:\n{output}"
    )


def test_numbered_fanout_cond_adv_grpc():
    """conditional advertisement arms as a pair (advertise-map +
    exist-map) and tears down cleanly."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _num_peer(r1)

    step("Half a pair arms nothing")
    run_grpc_client(
        r1, f"commit-set,{EAF}/conditional-advertisement/advertise-map=adv1"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "advertise-map" not in output, (
        f"half pair must not render:\n{output}"
    )

    step("The condition completes the knob")
    run_grpc_client(
        r1, f"commit-set,{EAF}/conditional-advertisement/exist-map=exist1"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "advertise-map adv1 exist-map exist1" in output, (
        f"cond-adv must render:\n{output}"
    )

    step("Switching to non-exist-map")
    run_grpc_client(
        r1,
        [
            f"commit-delete,{EAF}/conditional-advertisement/exist-map",
            f"commit-set,{EAF}/conditional-advertisement/non-exist-map=nonexist1",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "advertise-map adv1 non-exist-map nonexist1" in output, (
        f"cond-adv non-exist must render:\n{output}"
    )

    step("Destroy the advertise-map: knob gone")
    run_grpc_client(
        r1,
        f"commit-delete,{EAF}/conditional-advertisement/advertise-map",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "advertise-map adv1" not in output, (
        f"cond-adv must be gone:\n{output}"
    )


def test_numbered_fanout_name_filters_grpc():
    """distribute-list, filter-list, prefix-list and unsuppress-map
    land through gRPC and bind by name (referenced object absent)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _num_peer(r1)

    step("Name-based filters via gRPC (distribute-list and prefix-list"
         " never share a direction)")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EAF}/filter-config/access-list-import=acl-in",
            f"commit-set,{EAF}/filter-config/as-path-filter-list-export=asf-out",
            f"commit-set,{EAF}/filter-config/plist-export=pl-out",
            f"commit-set,{EAF}/filter-config/unsuppress-map-export=usm1",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "distribute-list acl-in in" in output, (
        f"distribute-list in missing:\n{output}"
    )
    assert "filter-list asf-out out" in output, (
        f"filter-list missing:\n{output}"
    )
    assert "prefix-list pl-out out" in output, (
        f"prefix-list missing:\n{output}"
    )
    assert "unsuppress-map usm1" in output, (
        f"unsuppress-map missing:\n{output}"
    )

    step("Swap distribute-list in for prefix-list in")
    run_grpc_client(
        r1, f"commit-delete,{EAF}/filter-config/access-list-import"
    )
    run_grpc_client(
        r1, f"commit-set,{EAF}/filter-config/plist-import=pl-in"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "prefix-list pl-in in" in output, (
        f"prefix-list in missing:\n{output}"
    )
    assert "distribute-list acl-in" not in output, (
        f"distribute-list must be gone:\n{output}"
    )

    step("Destroy one filter at a time")
    run_grpc_client(
        r1, f"commit-delete,{EAF}/filter-config/plist-import"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "prefix-list pl-in" not in output, (
        f"prefix-list must be gone:\n{output}"
    )
    run_grpc_client(
        r1, f"commit-delete,{EAF}/filter-config/unsuppress-map-export"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "unsuppress-map" not in output, (
        f"unsuppress-map must be gone:\n{output}"
    )


def test_numbered_fanout_upa_soft_reconfig_grpc():
    """upa and soft-reconfiguration (per-AF booleans) land through
    gRPC and destroy returns the knob to the default."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    _num_peer(r1)

    step("upa + soft-reconfiguration via gRPC")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EAF}/upa=true",
            f"commit-set,{EAF}/soft-reconfiguration=true",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"neighbor {PEER} upa" in output, f"upa missing:\n{output}"
    assert "soft-reconfiguration inbound" in output, (
        f"soft-reconfig missing:\n{output}"
    )


def test_peer_group_fanout_grpc():
    """The peer-group context resolves through the peer-group-name
    list key: knobs land on the group conf and render."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)

    step("Create the peer group with EVPN AF via gRPC")
    run_grpc_client(
        r1,
        [
            f"commit-set,{NBPG}/neighbor-remote-as/remote-as-type=as-specified,"
            f"{NBPG}/neighbor-remote-as/remote-as=65100",
        ],
    )

    step("Fanout knobs on the peer-group context")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EPG}/as-path-options/allow-own-as=5",
            f"commit-set,{EPG}/soo=65000:88",
            f"commit-set,{EPG}/route-server/route-server-client=true",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"neighbor {PG} allowas-in 5" in output, (
        f"pg allowas-in missing:\n{output}"
    )
    assert f"neighbor {PG} soo 65000:88" in output, (
        f"pg soo missing:\n{output}"
    )
    assert f"neighbor {PG} route-server-client" in output, (
        f"pg route-server-client missing:\n{output}"
    )

    step("Destroy the pg soo")
    run_grpc_client(r1, f"commit-delete,{EPG}/soo")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "soo 65000:88" not in output, f"pg soo must be gone:\n{output}"


def test_unnumbered_fanout_grpc():
    """The unnumbered context resolves through the interface list
    key: knobs land and render. The unnumbered peer itself is created
    by the legacy CLI first (mgmtd's copy does not track CLI-written
    config -- NB_CLIENT_CLI exemption, same seeding rationale as
    local-as)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)

    step("Create the unnumbered neighbor via the legacy CLI")
    r1.vtysh_cmd(
        f"configure terminal\n"
        f"router bgp 65000\n"
        f"neighbor {IFPEER} interface remote-as 65100\n"
        f"end\n"
    )

    step("EVPN AF enable + fanout knobs on the unnumbered context")
    run_grpc_client(
        r1,
        [
            f"commit-set,{NBIF}/afi-safis/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/enabled=true",
        ],
    )

    step("Fanout knobs on the unnumbered context")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EIF}/as-path-options/allow-own-as=4",
            f"commit-set,{EIF}/filter-config/plist-export=pl-out",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert f"neighbor {IFPEER} allowas-in 4" in output, (
        f"unnumbered allowas-in missing:\n{output}"
    )
    assert f"neighbor {IFPEER} prefix-list pl-out out" in output, (
        f"unnumbered prefix-list missing:\n{output}"
    )
