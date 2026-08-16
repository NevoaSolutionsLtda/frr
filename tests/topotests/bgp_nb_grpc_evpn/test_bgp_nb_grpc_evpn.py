# SPDX-License-Identifier: ISC
"""
Verifies the Fase C fatia 1 northbound wiring in bgpd: the global half
of the l2vpn-evpn address family (advertise-all-vni, flooding, DAD,
SoO, multihoming knobs, advertise-pip, ip-vrf RD/RT/type-5 and the vni
list) plus the EVPN prefix-limit fanout become programmable through
the mgmtd gRPC bridge.

RED on the base trunk (s057 head 045ff0b52): the commits hit
reject-strict stubs and fail. GREEN on the s058 head: the commits
apply through the same evpn_* internals the CLI DEFUNs call and the
legacy CLI surface (show running-config) renders the exact knob lines,
proving the datastore and the bgpd internals agree.

Functional dataplane proof (type-2/type-5 exchange on a 2-VTEP rig) is
fatia 2; here the G-EVPN-1 gate proves advertise-all-vni takes effect
(show bgp l2vpn evpn vni answers with EVPN enabled).
"""
import glob
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
EG = (
    f"{CPP}/global/afi-safis"
    "/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"
)
EG_ENTRY = (
    f"{CPP}/global/afi-safis"
    "/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']"
)
EVPN_AF = (
    f"{NB}/afi-safis"
    "/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']/l2vpn-evpn"
)
EVPN_AF_ENTRY = (
    f"{NB}/afi-safis"
    "/afi-safi[afi-safi-name='frr-routing:l2vpn-evpn']"
)
CPPV = (
    "/frr-routing:routing/control-plane-protocols/control-plane-protocol"
    "[type='frr-bgp:bgp'][name='bgp'][vrf='red']/frr-bgp:bgp"
)
EGV = (
    f"{CPPV}/global/afi-safis"
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


def _seed(r1, evpn=True):
    """Idempotent: seed local-as into the datastore (mgmtd's copy does
    not track CLI-written config -- NB_CLIENT_CLI exemption) and turn
    on advertise-all-vni (EVPN) so the per-knob validations pass."""
    updates = f"{CPP}/global/local-as=65000"
    if evpn:
        updates += f",{EG}/advertise-all-vni=true"
    # status variant: a no-op re-seed ("No changes found") is fine
    run_grpc_client_status(r1, f"commit-set,{updates}")


def test_advertise_all_vni_grpc():
    """G-EVPN-1: advertise-all-vni via gRPC lands in the internals,
    renders on the legacy CLI and turns EVPN on (show bgp l2vpn evpn
    vni answers)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Seed local-as into the datastore, then advertise-all-vni")
    run_grpc_client(
        r1,
        f"commit-set,{CPP}/global/local-as=65000,"
        f"{EG}/advertise-all-vni=true",
    )

    step("The legacy CLI shows advertise-all-vni")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "advertise-all-vni" in output, (
        f"expected advertise-all-vni on legacy CLI; got:\n{output}"
    )

    step("EVPN is effectively enabled (G-EVPN-1)")
    output = r1.vtysh_cmd("show bgp l2vpn evpn vni json")
    assert isinstance(output, str), "show bgp l2vpn evpn vni must answer"

    step("The gRPC get-config view agrees (round-trip)")
    out = run_grpc_client(r1, f"get-config,{EG}/advertise-all-vni")
    assert "true" in out, f"advertise-all-vni missing: {out}"


def test_globals_flooding_dad_soo_grpc():
    """flooding, dup-addr-detection, mac-vrf soo and
    enable-resolve-overlay-index land through gRPC."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    step("Commit flooding disable + DAD knobs + SoO")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EG}/flooding=disable",
            f"commit-set,{EG}/duplicate-address-detection"
            "/max-moves=10",
            f"commit-set,{EG}/duplicate-address-detection/time=200",
            f"commit-set,{EG}/duplicate-address-detection"
            "/freeze-time=300",
            f"commit-set,{EG}/mac-vrf-site-of-origin=65000:100",
            f"commit-set,{EG}/enable-resolve-overlay-index=true",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "flooding disable" in output, f"flooding missing:\n{output}"
    assert "dup-addr-detection max-moves 10 time 200" in output, (
        f"dad max-moves/time missing:\n{output}"
    )
    assert "dup-addr-detection freeze 300" in output, (
        f"dad freeze missing:\n{output}"
    )
    assert "mac-vrf soo 65000:100" in output, f"soo missing:\n{output}"
    assert "enable-resolve-overlay-index" in output, (
        f"resolve-overlay missing:\n{output}"
    )

    step("Destroy the SoO leaf")
    run_grpc_client(r1, f"commit-delete,{EG}/mac-vrf-site-of-origin")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "mac-vrf soo" not in output, f"soo must be gone:\n{output}"


def test_default_originate_grpc():
    """default-originate ipv4/ipv6 through gRPC."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1, evpn=False)
    run_grpc_client(
        r1,
        [
            f"commit-set,{EG}/default-originate/ipv4=true",
            f"commit-set,{EG}/default-originate/ipv6=true",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "default-originate ipv4" in output, (
        f"default-originate ipv4 missing:\n{output}"
    )
    assert "default-originate ipv6" in output, (
        f"default-originate ipv6 missing:\n{output}"
    )

    step("Turning both off withdraws them")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EG}/default-originate/ipv4=false",
            f"commit-set,{EG}/default-originate/ipv6=false",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "default-originate" not in output, (
        f"default-originate must be gone:\n{output}"
    )


def test_multihoming_knobs_grpc():
    """Multihoming global knobs through gRPC."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    step("Commit use-es-l3nhg, ead-evi rx/tx, ead-es-frag and EAD-ES RT")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EG}/multihoming/use-es-l3nhg=false",
            f"commit-set,{EG}/multihoming/disable-ead-evi-rx=true",
            f"commit-set,{EG}/multihoming/ead-es-fragment-evi-limit=64",
            f"commit-set,{EG}/multihoming/ead-es-export-route-target"
            "=65000:999",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "no use-es-l3nhg" in output, f"use-es-l3nhg missing:\n{output}"
    assert "disable-ead-evi-rx" in output, (
        f"disable-ead-evi-rx missing:\n{output}"
    )
    assert "ead-es-frag evi-limit 64" in output, (
        f"ead-es-frag missing:\n{output}"
    )
    assert "ead-es-route-target export 65000:999" in output, (
        f"ead-es-route-target missing:\n{output}"
    )

    step("Destroy the EAD-ES export route-target entry")
    run_grpc_client(
        r1,
        f"commit-delete,{EG}/multihoming/ead-es-export-route-target"
        "[.='65000:999']",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "ead-es-route-target" not in output, (
        f"ead-es-route-target must be gone:\n{output}"
    )


def test_vni_grpc():
    """The vni list (rd, import/export RT, flooding, advertise knobs)
    through gRPC."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    step("Create vni 100 with rd + RTs (leaf modify creates the entry)")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EG}/vni[vni='100']/rd=65000:100",
            f"commit-set,{EG}/vni[vni='100']/import-route-target"
            "=65000:200",
            f"commit-set,{EG}/vni[vni='100']/export-route-target"
            "=65000:201",
            f"commit-set,{EG}/vni[vni='100']/flooding=disable",
            f"commit-set,{EG}/vni[vni='100']/advertise-default-gateway"
            "=true",
            f"commit-set,{EG}/vni[vni='100']/advertise-svi-ip=true",
            f"commit-set,{EG}/vni[vni='100']/advertise-subnet=true",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "vni 100" in output, f"vni block missing:\n{output}"
    assert "rd 65000:100" in output, f"vni rd missing:\n{output}"
    assert "route-target import 65000:200" in output, (
        f"vni import RT missing:\n{output}"
    )
    assert "route-target export 65000:201" in output, (
        f"vni export RT missing:\n{output}"
    )
    assert "advertise-default-gw" in output, (
        f"vni advertise-default-gw missing:\n{output}"
    )

    step("Destroying the vni entry withdraws the whole block")
    run_grpc_client(r1, f"commit-delete,{EG}/vni[vni='100']")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "vni 100" not in output, f"vni block must be gone:\n{output}"


def test_vrf_rd_rt_type5_grpc():
    """ip-vrf rd/RT/auto-RT/type-5 on a VRF instance through gRPC."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the VRF instance through the legacy CLI (context only)")
    r1.vtysh_cmd(
        "configure terminal\nvrf definition red\nexit\n"
        "router bgp 65000 vrf red\nexit\n"
        "route-map RM permit 10\nend\n"
    )

    run_grpc_client(r1, f"commit-set,{CPPV}/global/local-as=65000")
    step("Commit rd + export RT + auto RT + type-5 knobs")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EGV}/ip-vrf/rd=65000:1000",
            f"commit-set,{EGV}/ip-vrf/export-route-target=65000:1001",
            f"commit-set,{EGV}/ip-vrf/export-route-target-auto=true",
            f"commit-set,{EGV}/ip-vrf/ipv4-unicast/enable=true",
            f"commit-set,{EGV}/ip-vrf/ipv4-unicast/gateway-ip=true",
            f"commit-set,{EGV}/ip-vrf/ipv4-unicast/route-map=RM",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "rd 65000:1000" in output, f"vrf rd missing:\n{output}"
    assert "route-target export 65000:1001" in output, (
        f"vrf export RT missing:\n{output}"
    )
    assert "advertise ipv4 unicast gateway-ip route-map RM" in output, (
        f"type-5 line missing:\n{output}"
    )

    step("Destroying the route-map leaf falls back to gateway-ip only")
    run_grpc_client(
        r1, f"commit-delete,{EGV}/ip-vrf/ipv4-unicast/route-map"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "gateway-ip route-map" not in output, (
        f"route-map must be cleared:\n{output}"
    )
    assert "gateway-ip" in output, (
        f"gateway-ip advertise must survive the rmap destroy:\n{output}"
    )


def test_vrf_pip_grpc():
    """advertise-pip enable/system-ip/system-mac through gRPC; the
    leaf destroy is surgical (destroying system-ip keeps system-mac)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    run_grpc_client(r1, f"commit-set,{CPPV}/global/local-as=65000")
    run_grpc_client(
        r1,
        [
            f"commit-set,{EGV}/advertise-pip/system-ip=1.1.1.1",
            f"commit-set,{EGV}/advertise-pip/system-mac"
            "=00:11:22:33:44:55",
        ],
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "1.1.1.1" in output, f"pip system-ip missing:\n{output}"
    assert "00:11:22:33:44:55" in output, (
        f"pip system-mac missing:\n{output}"
    )

    step("Destroying system-ip keeps the system-mac (leaf-surgical)")
    run_grpc_client(
        r1, f"commit-delete,{EGV}/advertise-pip/system-ip"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "1.1.1.1" not in output, f"pip ip must be gone:\n{output}"
    assert "00:11:22:33:44:55" in output, (
        f"pip mac must survive the ip destroy:\n{output}"
    )


def test_prefix_limit_evpn_grpc():
    """Fase B fanout: the EVPN prefix-limit lands through the shared
    callbacks and renders on the legacy CLI under l2vpn evpn."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Create the neighbor and activate l2vpn-evpn on it")
    run_grpc_client(
        r1,
        [
            f"commit-set,{CPP}/global/local-as=65000",
            f"commit-result,ALL,"
            f"{NB}/neighbor-remote-as/remote-as-type=as-specified,"
            f"{NB}/neighbor-remote-as/remote-as=65001,"
            f"{EVPN_AF_ENTRY}/enabled=true,"
            f"{EVPN_AF}/prefix-limit/direction-list"
            "[direction='in']/max-prefixes=3,"
            f"{EVPN_AF}/prefix-limit/direction-list"
            "[direction='in']/options/shutdown-threshold-pct=75",
        ],
    )

    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix 3 75" in output, (
        f"expected EVPN maximum-prefix on legacy CLI; got:\n{output}"
    )

    step("The CLI keeps authority: re-configure through vtysh")
    r1.vtysh_cmd(
        "configure terminal\nrouter bgp 65000\n"
        "address-family l2vpn evpn\n"
        f"neighbor {PEER} maximum-prefix 4\n"
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 maximum-prefix 4" in output, (
        f"CLI-configured EVPN maximum-prefix missing:\n{output}"
    )

    step("Destroy the direction-list through gRPC")
    run_grpc_client(
        r1,
        f"commit-delete,{EVPN_AF}/prefix-limit/direction-list"
        "[direction='in']",
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "maximum-prefix 4" not in output, (
        f"EVPN maximum-prefix must be gone:\n{output}"
    )


def test_autort_roundtrip_grpc():
    """autort-rfc8365-compatible round-trips through the datastore
    (deprecated CLI surface: no legacy show line is asserted)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    _seed(r1)
    run_grpc_client(
        r1, f"commit-set,{EG}/autort-rfc8365-compatible=true"
    )
    out = run_grpc_client(
        r1, f"get-config,{EG}/autort-rfc8365-compatible"
    )
    assert "true" in out, f"autort missing from datastore: {out}"

    run_grpc_client(
        r1, f"commit-set,{EG}/autort-rfc8365-compatible=false"
    )
    out = run_grpc_client(
        r1, f"get-config,{EG}/autort-rfc8365-compatible"
    )
    assert "false" in out, f"autort must read back false: {out}"
