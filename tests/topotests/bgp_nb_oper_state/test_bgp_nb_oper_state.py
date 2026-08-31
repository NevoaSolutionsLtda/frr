# SPDX-License-Identifier: ISC
"""
Verifies the B5.1 bgpd operational-state pull path: async Get(STATE)
and Subscribe through mgmtd serve the frr-bgp-oper subtrees
(instance/global/neighbor session state) for a legacy-configured
eBGP session.

RED on the base trunk (S065 spike, tree 31e6525a07): Get(STATE) on
the BGP subtree returns an empty body (rc=0), a keyed state-leaf
query fails with INVALID_ARGUMENT "Data path not found" (the schema
gate) and Subscribe(STREAM) serves an empty snapshot — bgpd
registered zero oper xpaths and zero state callbacks. GREEN with
yang/frr-bgp-oper.yang + bgpd/bgp_nb_oper.c (module tree mode) and
the real trunk-list iteration callbacks.
"""
import glob
import json
import os
import sys

import pytest
from lib.common_config import step
from lib.topogen import Topogen, TopoRouter, get_topogen
from lib.topotest import run_and_expect

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
NB_STATE = f"{CPP}/neighbors/neighbor[remote-address='{PEER}']/frr-bgp-oper:state"
INST_STATE = f"{CPP}/frr-bgp-oper:state"
GLOBAL_STATE = f"{CPP}/global/frr-bgp-oper:state"


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
    tgen.add_router("r2")
    switch = tgen.add_switch("s1")
    switch.add_link(tgen.gears["r1"])
    switch.add_link(tgen.gears["r2"])


def setup_module(mod):
    tgen = Topogen(build_topo, mod.__name__)
    tgen.start_topology()

    r1 = tgen.gears["r1"]
    r2 = tgen.gears["r2"]
    for rname, router in tgen.routers().items():
        router.load_config(
            TopoRouter.RD_ZEBRA, os.path.join(CWD, f"{rname}/zebra.conf")
        )
        router.load_config("bgpd", os.path.join(CWD, f"{rname}/bgpd.conf"))
    r1.load_config(TopoRouter.RD_MGMTD, "", f"-M grpc:{GRPCP_MGMTD}")

    tgen.start_router()


def teardown_module():
    tgen = get_topogen()
    tgen.stop_topology()


def run_grpc_client(r, commands):
    if not isinstance(commands, str):
        commands = "\n".join(commands) + "\n"
    if not commands.endswith("\n"):
        commands += "\n"
    return r.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=commands)


@pytest.fixture(autouse=True)
def skip_on_failure():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)


@pytest.fixture(scope="module")
def bgp_established():
    """Wait for the eBGP session (and r1's pfxRcd) before any query."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    def _converged():
        summary = json.loads(r1.vtysh_cmd("show bgp summary json"))
        peer = (
            summary.get("ipv4Unicast", {}).get("peers", {}).get(PEER, {})
        )
        return (
            peer.get("state") == "Established"
            and peer.get("pfxRcd", 0) >= 1
            and peer.get("msgRcvd", 0) >= 1
        )

    ok, summary = run_and_expect(_converged, True, count=60, wait=1)
    assert ok, f"eBGP session did not converge: {summary}"


def _get_state(r1, xpath):
    out = run_grpc_client(r1, f"get-state,{xpath}")
    return json.loads(out) if out.strip() else None


def test_oper_instance_state(bgp_established):
    """G-IS: broad Get(STATE) over the cpp container serves the bgp
    instance projection (E1 RED: empty body)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    step("Get(STATE) on the control-plane-protocols subtree")
    doc = _get_state(
        r1,
        "/frr-routing:routing/control-plane-protocols",
    )
    assert doc is not None, "Get(STATE) returned an empty body"

    cpps = doc["frr-routing:routing"]["control-plane-protocols"][
        "control-plane-protocol"
    ]
    bgp_inst = [
        c for c in cpps if c.get("type") == "frr-bgp:bgp"
        and c.get("vrf") == "default"
    ]
    assert len(bgp_inst) == 1, f"bgp instance missing: {json.dumps(doc)}"
    state = bgp_inst[0]["frr-bgp:bgp"]["frr-bgp-oper:state"]
    assert state["local-as"] == 65000, f"local-as wrong: {state}"
    assert state["router-id"], f"router-id missing: {state}"

    gstate = bgp_inst[0]["frr-bgp:bgp"]["global"]["frr-bgp-oper:state"]
    afs = {a["afi-safi-name"]: a for a in gstate["afi-safi"]}
    assert "frr-routing:ipv4-unicast" in afs, f"ipv4-unicast missing: {gstate}"
    assert int(afs["frr-routing:ipv4-unicast"]["rib-count"]) >= 1


def test_oper_neighbor_state(bgp_established):
    """G-NS: the neighbor session projection matches the summary
    counters (fsm-state, msg counters, pfxRcd, uptime)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    doc = _get_state(r1, CPP)
    assert doc is not None, "Get(STATE) returned an empty body"

    cpps = doc["frr-routing:routing"]["control-plane-protocols"][
        "control-plane-protocol"
    ]
    neighbors = cpps[0]["frr-bgp:bgp"]["neighbors"]["neighbor"]
    peer = [n for n in neighbors if n["remote-address"] == PEER]
    assert len(peer) == 1, f"neighbor {PEER} missing: {json.dumps(doc)}"
    state = peer[0]["frr-bgp-oper:state"]

    summary = json.loads(r1.vtysh_cmd("show bgp summary json"))
    ref = summary["ipv4Unicast"]["peers"][PEER]

    assert state["fsm-state"] == "Established", f"fsm-state: {state}"
    assert state["connections-established"] == ref["connectionsEstablished"]
    assert int(state["msg-rcvd"]) == ref["msgRcvd"]
    assert int(state["msg-snt"]) == ref["msgSent"]
    assert int(state["uptime-msec"]) > 0, f"uptime-msec: {state}"
    assert state["last-reset"], f"last-reset missing: {state}"

    afs = {a["afi-safi-name"]: a for a in state["afi-safi"]}
    assert "frr-routing:ipv4-unicast" in afs, f"peer afi-safi: {state}"
    assert int(afs["frr-routing:ipv4-unicast"]["pfx-rcd"]) == ref["pfxRcd"]
    assert int(afs["frr-routing:ipv4-unicast"]["pfx-snt"]) >= 1


def test_oper_keyed_neighbor_state(bgp_established):
    """G-KL: a keyed leaf query resolves through the trunk-list
    lookup (E4 RED: 'Data path not found')."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    doc = _get_state(r1, f"{NB_STATE}/fsm-state")
    assert doc is not None, "keyed Get(STATE) returned an empty body"
    cpp = doc["frr-routing:routing"]["control-plane-protocols"][
        "control-plane-protocol"
    ][0]
    leaf = (
        cpp["frr-bgp:bgp"]["neighbors"]["neighbor"][0]
        ["frr-bgp-oper:state"]["fsm-state"]
    )
    assert leaf == "Established", f"fsm-state leaf: {doc}"


def test_oper_state_paths(bgp_established):
    """G-SP: Get(STATE-PATHS) enumerates the served state leaves
    (responses may stream in any order)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    xpaths = (
        f"{INST_STATE}/router-id",
        f"{INST_STATE}/local-as",
        f"{GLOBAL_STATE}/afi-safi[afi-safi-name='frr-routing:ipv4-unicast']/rib-count",
        f"{NB_STATE}/fsm-state",
    )
    out = run_grpc_client(r1, f"get-state-paths,{';'.join(xpaths)}")
    segments = out.split("===RESPONSE===")
    assert len(segments) == len(xpaths), (
        f"expected {len(xpaths)} responses: {out}"
    )
    served = {}
    for seg in segments:
        lines = seg.strip().splitlines()
        assert lines, f"empty response segment: {seg!r}"
        served[lines[0]] = "\n".join(lines[1:]).strip()
    assert set(served) == set(xpaths), (
        f"served paths mismatch: {sorted(served)} vs {sorted(xpaths)}"
    )
    for xpath, data in served.items():
        assert data, f"no data served for {xpath}"


def test_oper_subscribe_until_sync(bgp_established):
    """G-SB: Subscribe(STREAM) initial snapshot serves the state
    subtree and reaches sync (E6 RED: empty update)."""
    tgen = get_topogen()
    r1 = tgen.gears["r1"]

    out = run_grpc_client(r1, f"subscribe-until-sync,{NB_STATE},10")
    assert '"sync_response": true' in out, f"sync missing: {out}"
    assert "fsm-state" in out and "Established" in out, f"state: {out}"
