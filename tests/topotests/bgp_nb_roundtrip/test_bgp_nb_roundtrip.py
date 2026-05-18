# SPDX-License-Identifier: ISC
"""
Verifies the bgpd <-> mgmtd round-trip: configuration written via mgmtd
becomes visible in `show running-config bgpd` and vice-versa.

Coverage:
  * router-id via mgmtd -> legacy CLI
  * router-id via legacy CLI -> mgmtd YANG view
  * neighbor passive round-trip
  * per-AF route-reflector-client round-trip
  * local-as apply_finish atomicity (single mgmt transaction)

The end-to-end round-trip currently depends on bgpd implementing NB
callbacks for every config node in the frr-bgp YANG tree
(nb_validate_callbacks() is fatal on missing entries). bgpd ships
with `ignore_cfg_cbs=true` on `frr_bgp_info` to avoid that fatal
exit while the migration is in progress, but the same flag also
short-circuits callback dispatch (lib/northbound.c
nb_callback_configuration() returns NB_OK early). Tests that drive
config via mgmtd are therefore marked xfail until bgpd ships
callbacks for every leaf or the validator gains a per-subtree skip
mechanism.
"""
import os
import sys
import pytest

CWD = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CWD, "../"))

from lib.topogen import Topogen, get_topogen
from lib.topolog import logger
from lib.common_config import step

pytestmark = [pytest.mark.bgpd, pytest.mark.mgmtd]

# All mgmtd-driven tests fail today because frr_bgp_info uses
# ignore_cfg_cbs=true (see module docstring). Mark them xfail
# (strict=False) so they show as expected-fail on CI and convert to
# pass once the migration completes.
_XFAIL_NB_DISPATCH = pytest.mark.xfail(
    reason="frr_bgp_info has ignore_cfg_cbs=true; mgmtd dispatch into "
           "bgpd is no-op until full callback coverage lands",
    strict=False,
)


CPP = (
    "/frr-routing:routing/control-plane-protocols/control-plane-protocol"
    "[type='frr-bgp:bgp'][name='bgp'][vrf='default']/frr-bgp:bgp"
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
    for rname, router in tgen.routers().items():
        router.load_config("mgmtd", os.path.join(CWD, f"{rname}/mgmtd.conf"))
        router.load_config("bgpd", os.path.join(CWD, f"{rname}/bgpd.conf"))
    tgen.start_router()


def teardown_module():
    tgen = get_topogen()
    tgen.stop_topology()


def mgmt_apply(router, *commands):
    """Run a list of `mgmt set-config` / etc. commands and apply.

    `configure terminal file-lock` acquires the candidate datastore
    lock for the duration of config-mode; without it, `mgmt commit
    apply` fails with 'source not locked'.
    """
    script = (
        "configure terminal file-lock\n"
        + "\n".join(commands)
        + "\nmgmt commit apply\n"
    )
    return router.vtysh_cmd(script)


@_XFAIL_NB_DISPATCH
def test_router_id_mgmtd_to_cli():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]
    step("Set router-id via mgmtd")
    # local-as is mandatory on the bgp container in frr-bgp.yang, so
    # any commit that touches the global subtree must also re-state
    # it. The initial value matches r1/bgpd.conf.
    mgmt_apply(
        r1,
        f'mgmt set-config {CPP}/global/local-as 65000',
        f'mgmt set-config {CPP}/global/router-id 10.0.0.1',
    )
    step("Verify legacy CLI shows the router-id")
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "bgp router-id 10.0.0.1" in output, (
        f"expected router-id on legacy CLI; got:\n{output}"
    )


@_XFAIL_NB_DISPATCH
def test_router_id_cli_to_mgmtd():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]
    r1.vtysh_cmd(
        "configure terminal\n"
        "router bgp 65000\n"
        " bgp router-id 10.0.0.2"
    )
    output = r1.vtysh_cmd(
        f'show mgmt get-config running-config-data xpath "{CPP}/global/router-id"'
    )
    assert "10.0.0.2" in output, (
        f"expected router-id in mgmtd YANG view; got:\n{output}"
    )


@_XFAIL_NB_DISPATCH
def test_neighbor_passive_roundtrip():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]
    r1.vtysh_cmd(
        "configure terminal\n"
        "router bgp 65000\n"
        " neighbor 10.0.0.2 remote-as 65001\n"
        " neighbor 10.0.0.2 passive"
    )
    xpath = (
        f"{CPP}/neighbors/neighbor[remote-address='10.0.0.2']/passive-mode"
    )
    output = r1.vtysh_cmd(
        f'show mgmt get-config running-config-data xpath "{xpath}"'
    )
    assert "true" in output.lower(), (
        f"expected passive-mode=true in YANG view; got:\n{output}"
    )


@_XFAIL_NB_DISPATCH
def test_per_af_route_reflector_client_roundtrip():
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]
    xpath = (
        f"{CPP}/neighbors/neighbor[remote-address='10.0.0.2']"
        "/afi-safis/afi-safi[afi-safi-name='frr-rt:ipv4-unicast']"
        "/route-reflector-client"
    )
    mgmt_apply(r1, f'mgmt set-config {xpath} true')
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 route-reflector-client" in output, (
        f"expected RR-client on legacy CLI; got:\n{output}"
    )


@_XFAIL_NB_DISPATCH
def test_local_as_apply_finish_roundtrip():
    """local-as is a multi-leaf apply_finish container — all three leaves
    must apply atomically in one mgmtd transaction."""
    tgen = get_topogen()
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]
    base = (
        f"{CPP}/neighbors/neighbor[remote-address='10.0.0.2']/local-as"
    )
    mgmt_apply(
        r1,
        f'mgmt set-config {base}/local-as 65999',
        f'mgmt set-config {base}/no-prepend true',
        f'mgmt set-config {base}/replace-as true',
    )
    output = r1.vtysh_cmd("show running-config bgpd")
    assert "neighbor 10.0.0.2 local-as 65999 no-prepend replace-as" in output, (
        f"expected full local-as line; got:\n{output}"
    )


if __name__ == "__main__":
    args = ["-s"] + sys.argv[1:]
    sys.exit(pytest.main(args))
