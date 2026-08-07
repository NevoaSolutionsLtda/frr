# SPDX-License-Identifier: ISC
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
#
# Copyright (C) 2026  Eric Parsonage
#
"""
Test mgmtd gRPC Subscribe streaming of YANG notifications.

Two routers run RIP with matching authentication strings.  At runtime r1's
authentication string is changed so the next RIP packet from r2 fires a
frr-ripd authentication notification inside ripd.  mgmtd receives the backend
notification, selects the gRPC subscriber through the frontend selector tree,
and streams the encoded notification payload to the connected client.
"""

import glob
import json
import os
import threading
import time

import pytest
from lib.micronet import commander
from lib.topogen import Topogen, TopoRouter

CWD = os.path.dirname(os.path.realpath(__file__))
GRPCP_MGMTD = 50058
GRPC_SUBSCRIBE_TEST_PENDING_LIMIT = 4
script_path = os.path.realpath(os.path.join(CWD, "../lib/grpc-query.py"))

pytestmark = [pytest.mark.ripd, pytest.mark.mgmtd]


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
            router.load_config(
                TopoRouter.RD_MGMTD,
                "",
                f"-M grpc:{GRPCP_MGMTD},{GRPC_SUBSCRIBE_TEST_PENDING_LIMIT}",
            )

    tgen.start_router()
    yield tgen
    tgen.stop_topology()


@pytest.fixture(autouse=True)
def skip_on_failure(tgen):
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)


def _set_auth(router, key):
    "Set rip authentication string on router's first interface."
    conf = (
        "conf t\n"
        f"interface {router.name}-eth0\n"
        f"ip rip authentication string {key}\n"
    )
    router.net.cmd_raises("vtysh", stdin=conf)


def _rollback(router, args):
    "Run a mgmt rollback command through vtysh's config node."
    conf = f"conf t\nmgmt rollback {args}\n"
    return router.net.cmd_raises("vtysh", stdin=conf)


def _commit_auth(router, key):
    "Set rip authentication string through mgmtd gRPC."
    path = (
        "/frr-interface:lib"
        f"/interface[name='{router.name}-eth0']"
        "/frr-ripd:rip/authentication-password"
    )
    cmd = f"COMMIT-SET,{path}={key}\n"
    return router.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_listen(r, xpath, timeout=15):
    cmd = f"SUBSCRIBE-LISTEN,{xpath},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_listen_with_path(r, xpath, timeout=15):
    cmd = f"SUBSCRIBE-LISTEN-WITH-PATH,{xpath},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_until_sync(r, xpath, timeout=15):
    cmd = f"SUBSCRIBE-UNTIL-SYNC,{xpath},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_until_heartbeat(r, xpath, heartbeat_ms=200, timeout=5):
    cmd = f"SUBSCRIBE-UNTIL-HEARTBEAT,{xpath},{heartbeat_ms},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_cancel(r, xpath, delay=0.5, timeout=5):
    cmd = f"SUBSCRIBE-CANCEL,{xpath},{delay},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_expect_shutdown(r, xpath, timeout=15):
    cmd = f"SUBSCRIBE-EXPECT-SHUTDOWN,{xpath},{timeout}\n"
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


def _run_typed_expect_error(r, mode, xpath, snapshot_type, expected, timeout=5):
    cmd = (
        "SUBSCRIBE-TYPED-EXPECT-ERROR,"
        f"{mode},{xpath},{snapshot_type},{expected},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_expect_error(r, mode, xpath, expected, timeout=5):
    cmd = f"SUBSCRIBE-EXPECT-ERROR,{mode},{xpath},{expected},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_sample_expect_error(r, xpath, interval_ms, expected, timeout=5):
    cmd = (
        "SUBSCRIBE-SAMPLE-EXPECT-ERROR," f"{xpath},{interval_ms},{expected},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_stream_repeat_expect_error(r, xpath, repeat, expected, timeout=5):
    cmd = (
        "SUBSCRIBE-STREAM-REPEAT-EXPECT-ERROR,"
        f"{xpath},{repeat},{expected},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_invalid_encoding_expect_error(r, mode, xpath, expected, timeout=5):
    cmd = (
        "SUBSCRIBE-INVALID-ENCODING-EXPECT-ERROR,"
        f"{mode},{xpath},99,{expected},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def test_subscribe_receives_rip_auth_notification(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_listen(r1, "/frr-ripd", timeout=30)

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _set_auth(r1, "bar")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no notification"

    data = json.loads(raw.splitlines()[-1])
    assert set(data) & {
        "frr-ripd:authentication-failure",
        "frr-ripd:authentication-type-failure",
    }, f"unexpected notification payload: {data}"

    _set_auth(r1, "foo")


def test_subscribe_update_includes_notification_path(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_listen_with_path(r1, "/frr-ripd", timeout=30)

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _set_auth(r1, "bar")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no notification"

    update = json.loads(raw.splitlines()[-1])
    assert update["path"] in {
        "/frr-ripd:authentication-failure",
        "/frr-ripd:authentication-type-failure",
    }, f"unexpected notification path: {update}"
    data = json.loads(update["data"])
    assert set(data) & {
        "frr-ripd:authentication-failure",
        "frr-ripd:authentication-type-failure",
    }, f"unexpected notification payload: {data}"

    _set_auth(r1, "foo")


def test_commit_config_then_subscribe_receives_notification(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_listen_with_path(r1, "/frr-ripd", timeout=30)

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _commit_auth(r1, "bar")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no notification"

    update = json.loads(raw.splitlines()[-1])
    assert update["path"] in {
        "/frr-ripd:authentication-failure",
        "/frr-ripd:authentication-type-failure",
    }, f"unexpected notification path: {update}"
    data = json.loads(update["data"])
    assert set(data) & {
        "frr-ripd:authentication-failure",
        "frr-ripd:authentication-type-failure",
    }, f"unexpected notification payload: {data}"

    _commit_auth(r1, "foo")


def test_stream_sends_initial_state_and_sync(tgen):
    r1 = tgen.gears["r1"]
    # STREAM snapshots use mgmtd-local operational state in this test.
    raw = _run_until_sync(
        r1,
        "/frr-backend:clients",
        timeout=15,
    ).strip()

    responses = json.loads(raw.splitlines()[-1])
    assert responses, "STREAM Subscribe returned no responses"
    assert responses[-1] == {"sync_response": True}
    updates = [item for item in responses if "update" in item]
    assert updates, "STREAM Subscribe returned no initial state update"
    assert all(update["path"] == "/frr-backend:clients" for update in updates)
    assert any("frr-backend:clients" in update["update"] for update in updates)


def test_subscribe_rejects_empty_path_list(tgen):
    r1 = tgen.gears["r1"]

    assert "INVALID_ARGUMENT" in _run_expect_error(
        r1, "ON_CHANGE", "", "INVALID_ARGUMENT"
    )


def test_subscribe_rejects_unknown_selector(tgen):
    r1 = tgen.gears["r1"]

    assert "INVALID_ARGUMENT" in _run_expect_error(
        r1,
        "ON_CHANGE",
        "/frr-does-not-exist:notification",
        "INVALID_ARGUMENT",
    )


def test_sample_rejects_subminimum_interval(tgen):
    r1 = tgen.gears["r1"]

    assert "INVALID_ARGUMENT" in _run_sample_expect_error(
        r1, "/frr-ripd", 99, "INVALID_ARGUMENT"
    )


def test_sample_sends_periodic_state(tgen):
    r1 = tgen.gears["r1"]

    # SAMPLE reads the same mgmtd-local operational state path repeatedly.
    raw = _run_sample_count(
        r1,
        "/frr-backend:clients",
        interval_ms=200,
        count=3,
        timeout=5,
    ).strip()

    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 3
    assert all(response["path"] == "/frr-backend:clients" for response in responses)
    assert all("frr-backend:clients" in response["data"] for response in responses)


def _assert_config_snapshot_responses(responses, xpath):
    assert len(responses) >= 3
    assert all(response["path"] == xpath for response in responses)
    for response in responses:
        data = json.loads(response["data"])
        interfaces = data["frr-interface:lib"]["interface"]
        eth0 = [entry for entry in interfaces if entry["name"] == "r1-eth0"]
        assert eth0, f"interface config missing from snapshot: {data}"
        rip = eth0[0].get("frr-ripd:rip", {})
        assert (
            "authentication-password" in rip
        ), f"rip config missing from snapshot: {eth0[0]}"


def test_sample_config_snapshot_streams_config_subtree(tgen):
    r1 = tgen.gears["r1"]

    # CONFIG snapshots read the mgmtd-local running datastore, the same
    # path Get(CONFIG) serves.
    raw = _run_sample_count_typed(
        r1,
        "/frr-interface:lib",
        "CONFIG",
        interval_ms=200,
        count=3,
        timeout=5,
    ).strip()

    _assert_config_snapshot_responses(
        json.loads(raw.splitlines()[-1]), "/frr-interface:lib"
    )


def test_sample_all_snapshot_serves_config(tgen):
    r1 = tgen.gears["r1"]

    # DataType.ALL == 0: this request differs from the untyped one only by
    # explicit field presence.  It must serve config (merged with state)
    # instead of failing like the STATE default does on a config path.
    raw = _run_sample_count_typed(
        r1,
        "/frr-interface:lib",
        "ALL",
        interval_ms=200,
        count=3,
        timeout=5,
    ).strip()

    _assert_config_snapshot_responses(
        json.loads(raw.splitlines()[-1]), "/frr-interface:lib"
    )


def test_sample_explicit_state_type_matches_default(tgen):
    r1 = tgen.gears["r1"]

    raw = _run_sample_count_typed(
        r1,
        "/frr-backend:clients",
        "STATE",
        interval_ms=200,
        count=3,
        timeout=5,
    ).strip()

    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 3
    assert all(response["path"] == "/frr-backend:clients" for response in responses)
    assert all("frr-backend:clients" in response["data"] for response in responses)


def test_sample_state_default_serves_backend_state_without_config(tgen):
    r1 = tgen.gears["r1"]

    # Without snapshot_type the snapshot serves STATE.  mgmtd now
    # dispatches backend operational state into SAMPLE snapshots, so a
    # config-owning path streams the zebra-owned state subtree -- but it
    # must not include config, which only an explicit ALL request merges.
    raw = _run_sample_count(
        r1,
        "/frr-interface:lib",
        interval_ms=200,
        count=3,
        timeout=5,
    ).strip()

    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 3
    for response in responses:
        assert response["path"] == "/frr-interface:lib"
        data = json.loads(response["data"])
        interfaces = data["frr-interface:lib"]["interface"]
        eth0 = [entry for entry in interfaces if entry["name"] == "r1-eth0"]
        assert eth0, f"interface state missing from snapshot: {data}"
        assert "if-index" in eth0[0]["state"], f"no state in snapshot: {eth0[0]}"
        rip = eth0[0].get("frr-ripd:rip", {})
        assert (
            "authentication-password" not in rip
        ), f"config leaked into a STATE snapshot: {eth0[0]}"


def test_on_change_rejects_snapshot_type(tgen):
    r1 = tgen.gears["r1"]

    assert "INVALID_ARGUMENT" in _run_typed_expect_error(
        r1, "ON_CHANGE", "/frr-ripd", "CONFIG", "INVALID_ARGUMENT"
    )


def test_stream_closes_when_pending_queue_limit_is_hit(tgen):
    r1 = tgen.gears["r1"]

    assert "OUT_OF_RANGE" in _run_stream_repeat_expect_error(
        r1,
        "/frr-backend:clients",
        GRPC_SUBSCRIBE_TEST_PENDING_LIMIT + 2,
        "OUT_OF_RANGE",
    )


def test_subscribe_heartbeat_on_quiet_stream(tgen):
    r1 = tgen.gears["r1"]

    assert "heartbeat" in _run_until_heartbeat(
        r1, "/frr-ripd", heartbeat_ms=200, timeout=5
    )


def test_subscribe_client_cancel_cleans_up_stream(tgen):
    r1 = tgen.gears["r1"]

    assert "CANCELLED" in _run_cancel(r1, "/frr-ripd")
    assert "heartbeat" in _run_until_heartbeat(
        r1, "/frr-ripd", heartbeat_ms=200, timeout=5
    )


@pytest.mark.parametrize("mode", ["POLL"])
def test_subscribe_rejects_unsupported_modes(tgen, mode):
    r1 = tgen.gears["r1"]

    assert "UNIMPLEMENTED" in _run_expect_error(r1, mode, "/frr-ripd", "UNIMPLEMENTED")


def test_subscribe_rejects_unknown_encoding(tgen):
    r1 = tgen.gears["r1"]

    assert "INVALID_ARGUMENT" in _run_invalid_encoding_expect_error(
        r1, "ON_CHANGE", "/frr-ripd", "INVALID_ARGUMENT"
    )


def test_subscribe_selector_does_not_overmatch(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_expect_error(
            r1, "ON_CHANGE", "/frr-backend:clients", "DEADLINE_EXCEEDED"
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(1)
    _set_auth(r1, "baz")

    t.join(timeout=10)
    assert not t.is_alive(), "non-matching Subscribe listener did not time out"
    assert "DEADLINE_EXCEEDED" in received.get("raw", "")
    _set_auth(r1, "foo")


def test_commit_emits_netconf_config_change(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_listen_with_path(
            r1, "/ietf-netconf-notifications:netconf-config-change", timeout=30
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _commit_auth(r1, "bar")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no netconf-config-change"

    update = json.loads(raw.splitlines()[-1])
    assert (
        update["path"] == "/ietf-netconf-notifications:netconf-config-change"
    ), f"unexpected notification path: {update}"
    data = json.loads(update["data"])
    change = data["ietf-netconf-notifications:netconf-config-change"]
    changed_by = change["changed-by"]
    # gRPC commits are attributed to the bridge identity: the service
    # performs no authentication and the synthetic gRPC session ids do
    # not fit the RFC 6470 uint32 session-id, which reserves 0 for
    # non-NETCONF sessions.
    assert changed_by.get("username") == "grpc", f"changed-by is not grpc: {change}"
    assert changed_by.get("session-id") == 0, f"session-id is not 0: {change}"
    assert "server" not in changed_by, f"unexpected server attribution: {change}"
    assert change.get("datastore", "running") == "running"
    edits = change["edit"]
    assert edits, f"netconf-config-change carried no edits: {change}"
    matching = [
        edit
        for edit in edits
        if edit["target"].endswith("/frr-ripd:rip/authentication-password")
        and "interface[name='r1-eth0']" in edit["target"]
    ]
    assert matching, f"expected edit target missing: {edits}"
    assert matching[0]["operation"] == "replace", f"unexpected operation: {matching}"

    _commit_auth(r1, "foo")


def test_vty_commit_attributes_config_change_to_session(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # Seed the known baseline instead of relying on the previous test's
    # cleanup having restored it.
    _set_auth(r1, "foo")

    def listener():
        received["raw"] = _run_listen_with_path(
            r1, "/ietf-netconf-notifications:netconf-config-change", timeout=30
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _set_auth(r1, "bar")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no netconf-config-change"

    update = json.loads(raw.splitlines()[-1])
    data = json.loads(update["data"])
    change = data["ietf-netconf-notifications:netconf-config-change"]
    changed_by = change["changed-by"]
    # vtysh commits ride mgmtd's vty front-end client, whose adapter is
    # named "vty-<progname>-<pid>", and their real front-end session ids
    # fit the RFC 6470 uint32 session-id: this is the control proving
    # native sessions are attributed distinctly from the gRPC bridge.
    username = changed_by.get("username", "")
    assert username.startswith("vty-mgmtd-"), f"unexpected username: {change}"
    assert changed_by.get("session-id", 0) != 0, f"session-id is 0: {change}"
    assert "server" not in changed_by, f"unexpected server attribution: {change}"

    _set_auth(r1, "foo")


def test_commit_without_changes_emits_no_config_change(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # Seed the known baseline instead of relying on the previous test's
    # cleanup having restored it.
    _set_auth(r1, "foo")

    def listener():
        received["raw"] = _run_expect_error(
            r1,
            "ON_CHANGE",
            "/ietf-netconf-notifications:netconf-config-change",
            "DEADLINE_EXCEEDED",
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(1)

    # Same value as running config: the commit is rejected with ABORTED
    # (MGMTD_NO_CFG_CHANGES) and no notification may be emitted.
    path = (
        "/frr-interface:lib"
        "/interface[name='r1-eth0']"
        "/frr-ripd:rip/authentication-password"
    )
    rc, _, err = r1.net.cmd_status(
        [script_path, f"--port={GRPCP_MGMTD}"], stdin=f"COMMIT-SET,{path}=foo\n"
    )
    assert rc != 0, "no-change commit unexpectedly succeeded"
    assert "No changes found" in err, f"unexpected commit error: {err}"

    t.join(timeout=10)
    assert not t.is_alive(), "no-change Subscribe listener did not time out"
    assert "DEADLINE_EXCEEDED" in received.get("raw", "")


def test_rollback_emits_netconf_config_change_with_initiator(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # Two known commits through the gRPC bridge so the rollback diff is
    # deterministic: the rollback target snapshots "s018base" while
    # running holds "s018new".  vtysh per-command implicit commits skip
    # the commit history, so the records must come from explicit commits.
    _commit_auth(r1, "s018base")
    _commit_auth(r1, "s018new")

    def listener():
        received["raw"] = _run_listen_with_path(
            r1, "/ietf-netconf-notifications:netconf-config-change", timeout=30
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(2)

    _rollback(r1, "last")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not return in time"

    raw = received.get("raw", "").strip()
    assert raw, "Subscribe stream returned no netconf-config-change"

    update = json.loads(raw.splitlines()[-1])
    data = json.loads(update["data"])
    change = data["ietf-netconf-notifications:netconf-config-change"]
    changed_by = change["changed-by"]
    # The rollback initiator is the vty session that ran the command: it
    # rides mgmtd's vty front-end client like any vtysh commit, so the
    # event reports that client name and the initiating session id even
    # though the rollback transaction itself is internal.
    username = changed_by.get("username", "")
    assert username.startswith("vty-mgmtd-"), f"unexpected username: {change}"
    assert changed_by.get("session-id", 0) != 0, f"session-id is 0: {change}"
    assert "server" not in changed_by, f"unexpected server attribution: {change}"
    edits = change["edit"]
    assert edits, f"netconf-config-change carried no edits: {change}"
    matching = [
        edit
        for edit in edits
        if edit["target"].endswith("/frr-ripd:rip/authentication-password")
        and "interface[name='r1-eth0']" in edit["target"]
    ]
    assert matching, f"expected edit target missing: {edits}"
    assert matching[0]["operation"] == "replace", f"unexpected operation: {matching}"

    # The rollback must have restored the target snapshot's value.
    out = r1.net.cmd_raises("vtysh", stdin="show running-config\n")
    assert "ip rip authentication string s018base" in out

    _set_auth(r1, "foo")


def test_rollback_without_changes_emits_no_config_change(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # Seed a history record whose snapshot matches the running config:
    # rolling back to it yields an empty diff, the command fails before a
    # transaction exists, and no notification may be emitted.
    _commit_auth(r1, "s018nc")
    out = r1.net.cmd_raises("vtysh", stdin="show mgmt commit-history\n")
    newest = None
    for line in out.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == "0":
            newest = fields[1]
            break
    assert newest, f"could not find newest commit id: {out}"

    def listener():
        received["raw"] = _run_expect_error(
            r1,
            "ON_CHANGE",
            "/ietf-netconf-notifications:netconf-config-change",
            "DEADLINE_EXCEEDED",
        )

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(1)

    out = _rollback(r1, f"commit-id {newest}")
    assert (
        "Error with creating commit apply txn" in out
    ), f"unexpected rollback output: {out}"

    t.join(timeout=10)
    assert not t.is_alive(), "no-change Subscribe listener did not time out"
    assert "DEADLINE_EXCEEDED" in received.get("raw", "")

    _set_auth(r1, "foo")


NETCONF_CHANGE = "/ietf-netconf-notifications:netconf-config-change"


def _zebra_signal(r, sig):
    "Stop or resume zebra to stall backend snapshot collection."
    r.cmd_raises(f"kill -{sig} $(cat /var/run/frr/zebra.pid)")


def _commit_rip_distance(router, value):
    "Commit a ripd-only config change: zebra is not an interested backend."
    path = "/frr-ripd:ripd/instance[vrf='default']/distance/default"
    cmd = f"COMMIT-SET,{path}={value}\n"
    return router.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_sample_cancel(r, xpath, interval_ms=200, delay=0.5, timeout=5):
    cmd = f"SUBSCRIBE-SAMPLE-CANCEL,{xpath},{interval_ms},{delay},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_stream_paths_order(r, xpaths, timeout=15):
    cmd = f"SUBSCRIBE-STREAM-PATHS-ORDER,{';'.join(xpaths)},{timeout}\n"
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def _run_stream_paths_expect_error(r, xpaths, expected, timeout=15):
    cmd = (
        "SUBSCRIBE-STREAM-PATHS-EXPECT-ERROR,"
        f"{';'.join(xpaths)},{expected},{timeout}\n"
    )
    return r.net.cmd_raises([script_path, f"--port={GRPCP_MGMTD}"], stdin=cmd)


def test_stream_backend_stall_closes_deadline_exceeded(tgen):
    r1 = tgen.gears["r1"]

    # The STREAM initial snapshot is the baseline the deltas build on:
    # when the owning backend cannot answer within the 10 s snapshot
    # deadline the stream must close instead of delivering a holed
    # baseline.  The elapsed bound proves the server closed the stream
    # (~10 s) rather than the client deadline expiring (20 s).
    _zebra_signal(r1, "STOP")
    started = time.time()
    try:
        assert "DEADLINE_EXCEEDED" in _run_expect_error(
            r1, "STREAM", "/frr-interface:lib", "DEADLINE_EXCEEDED", timeout=20
        )
    finally:
        _zebra_signal(r1, "CONT")
    assert time.time() - started < 16, "stream was not closed server-side"


def test_sample_backend_stall_delivers_partial_and_heals(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # A sampling snapshot is bounded by min(interval, 10 s): a timed-out
    # read delivers whatever merged before the deadline (an empty update
    # here, zebra never answered) and keeps the stream.  Ticks landing
    # mid-collection coalesce, and the first tick after the backend
    # recovers self-heals.
    def collector():
        received["raw"] = _run_sample_count(
            r1, "/frr-interface:lib", interval_ms=300, count=12, timeout=30
        )

    t = threading.Thread(target=collector, daemon=True)
    t.start()
    time.sleep(1.2)
    _zebra_signal(r1, "STOP")
    try:
        time.sleep(2.4)
    finally:
        _zebra_signal(r1, "CONT")

    t.join(timeout=35)
    assert not t.is_alive(), "SAMPLE collector did not return in time"

    responses = json.loads(received["raw"].strip().splitlines()[-1])
    assert len(responses) >= 12
    assert '"if-index"' in responses[0]["data"], "no healthy sample before the stall"
    empties = [r for r in responses if not r["data"]]
    assert empties, "no partial sample delivered during the stall"
    # A tick pile-up would emit roughly one partial per interval (~8 for
    # a 2.4 s stall at 300 ms); coalescing bounds them to one per
    # timeout-plus-rearm window (~4).
    assert len(empties) <= 7, f"tick pile-up during the stall: {len(empties)}"
    assert '"if-index"' in responses[-1]["data"], "stream did not heal after the stall"


def test_sample_cancel_mid_collection_cleans_up(tgen):
    r1 = tgen.gears["r1"]

    # Cancel while the snapshot collection is stalled inside the backend:
    # the in-flight dispatch must detach cleanly and the server must keep
    # serving new subscriptions afterwards.
    _zebra_signal(r1, "STOP")
    try:
        assert "CANCELLED" in _run_sample_cancel(
            r1, "/frr-interface:lib", interval_ms=1000, delay=0.5, timeout=5
        )
    finally:
        _zebra_signal(r1, "CONT")

    raw = _run_sample_count(
        r1, "/frr-interface:lib", interval_ms=200, count=2, timeout=5
    ).strip()
    responses = json.loads(raw.splitlines()[-1])
    assert len(responses) >= 2
    assert '"if-index"' in responses[0]["data"]


def test_stream_side_buffer_orders_baseline_before_deltas(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # A notification firing while the STREAM baseline is still collecting
    # must be held back and delivered after sync_response (at-least-once
    # post-baseline), never interleaved with the baseline.
    _commit_rip_distance(r1, 120)

    def collector():
        received["raw"] = _run_stream_paths_order(
            r1, ["/frr-interface:lib", NETCONF_CHANGE], timeout=20
        )

    _zebra_signal(r1, "STOP")
    try:
        t = threading.Thread(target=collector, daemon=True)
        t.start()
        # The stalled collection holds the baseline for 10 s: wait out the
        # client startup so the commit lands mid-collection.
        time.sleep(2.5)
        _commit_rip_distance(r1, 121)
        time.sleep(0.5)
    finally:
        _zebra_signal(r1, "CONT")

    t.join(timeout=25)
    assert not t.is_alive(), "STREAM order collector did not return in time"

    events = json.loads(received["raw"].strip().splitlines()[-1])
    assert {"sync_response": True} in events, f"no sync_response: {events}"
    sync_idx = events.index({"sync_response": True})
    baseline = events[:sync_idx]
    assert baseline, f"no baseline updates before sync_response: {events}"
    assert all(
        e["update"] in {"/frr-interface:lib", NETCONF_CHANGE} for e in baseline
    ), f"unexpected baseline update: {events}"
    assert any(
        e["update"] == "/frr-interface:lib" and not e["empty"] for e in baseline
    ), f"baseline missing interface data: {events}"
    after = events[sync_idx + 1 :]
    assert after, f"no delta delivered after sync_response: {events}"
    assert (
        after[0]["update"] == NETCONF_CHANGE and not after[0]["empty"]
    ), f"post-baseline delta is not the held notification: {events}"


def test_stream_side_buffer_overflow_closes_out_of_range(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    # Held deltas share the pending-queue budget (limit 4 here): a client
    # whose baseline cannot complete while notifications keep firing is a
    # slow consumer and must close OUT_OF_RANGE.
    _commit_rip_distance(r1, 110)

    def collector():
        received["raw"] = _run_stream_paths_expect_error(
            r1, ["/frr-interface:lib", NETCONF_CHANGE], "OUT_OF_RANGE", timeout=20
        )

    _zebra_signal(r1, "STOP")
    try:
        t = threading.Thread(target=collector, daemon=True)
        t.start()
        # The stalled collection holds the baseline for 10 s: wait out the
        # client startup so the commits land mid-collection.
        time.sleep(2.5)
        for value in (111, 112, 113, 114, 115):
            _commit_rip_distance(r1, value)
    finally:
        _zebra_signal(r1, "CONT")

    t.join(timeout=25)
    assert not t.is_alive(), "STREAM overflow collector did not return in time"
    assert "OUT_OF_RANGE" in received["raw"]


def test_subscribe_closes_cleanly_when_mgmtd_stops(tgen):
    r1 = tgen.gears["r1"]
    received = {}

    def listener():
        received["raw"] = _run_expect_shutdown(r1, "/frr-ripd", timeout=30)

    t = threading.Thread(target=listener, daemon=True)
    t.start()
    time.sleep(1)

    r1.cmd_raises("kill -TERM $(cat /var/run/frr/mgmtd.pid)")

    t.join(timeout=35)
    assert not t.is_alive(), "Subscribe listener did not close after mgmtd stop"
    assert received.get("raw", "").strip() in {"CANCELLED", "UNAVAILABLE"}

    with open(os.path.join(tgen.logdir, "r1", "mgmtd.log"), encoding="utf-8") as log:
        contents = log.read()
        assert "Terminating on signal" in contents
        assert "Received signal 11" not in contents
