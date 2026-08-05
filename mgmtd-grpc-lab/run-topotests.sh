#!/usr/bin/env bash
# Run the gRPC topotest suites from a mounted FRR checkout inside the
# frr-22158-test container. Expects: source at /src/frr, container
# privileged, host with mpls modules loaded.
set -uo pipefail
SRC=${SRC:-/src/frr}
pip3 install --break-system-packages -q micronet munet 2>/dev/null || \
  pip3 install -q micronet munet
cd "$SRC/tests/topotests"
for suite in grpc_basic mgmtd_grpc_rpc mgmtd_grpc_notif; do
  echo "== $suite"
  timeout 900 python3 -m pytest "$suite" -q 2>&1 | tail -4
done
