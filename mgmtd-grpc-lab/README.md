# mgmtd gRPC lab — reproducible test notes for PR #22158

Independent test run of https://github.com/FRRouting/frr/pull/22158
(mgmtd-centred gRPC Get/Commit/Execute/Subscribe), plus one data point on
the legacy per-daemon gRPC path in 10.5.3.

Tested tree: `lamestllama/frr-ospf-native-yang` branch `grpc-subscribe`,
commit `e79079be`. Built from source in a podman container (debian
bookworm) with:

```
--enable-grpc --enable-mgmtd --enable-mgmtd-test-be-client
```

(see `Dockerfile` and `run-topotests.sh` for the full recipe; libyang built
from source at v2.1.148, same as the fork doc recommends).

## Results on e79079be (this PR's own topotests)

| suite | result |
|---|---|
| `mgmtd_grpc_rpc` | 11/11 pass (Get/Commit/Execute, concurrent Execute, cancellation) |
| `mgmtd_grpc_notif` | 15/15 pass (Subscribe, initial snapshot, sync markers, heartbeats) |
| `grpc_basic` | 2 pass, 3 skipped, 1 fail |

The one failure, reproducible:

```
test_basic_grpc.py::test_get_config
> $: expected has key 'frr-logging:logging' which is not present in output
```

GET of the running datastore returns everything except the
`frr-logging:logging` subtree, even with `log timestamp precision 6` in
frr.conf. Raw output in `results/topotests-e79079be.txt`.

Environment notes (things that had to be in place for the suites to run,
for anyone reproducing): mpls kernel modules loaded on the host
(`mpls_router`, `mpls_iptunnel`, `mpls_gso`), `gdb` installed, and the
installed tree exposed at `/usr/lib/frr` (daemons + modules) since
topotests look there.

## Data point on the legacy per-daemon gRPC (FRR 10.5.3)

Separate container, stock `frr-10.5.3`, `zebra + mgmtd + staticd -M grpc`:

- `commit-set` of a static route over the per-daemon gRPC returns success
  and the route appears nowhere (not in `show running-config`, not in the
  RIB). `get-config /frr-staticd:staticd` answers "Data path not found"
  for a module the daemon itself serves.
- the same change over `vtysh` CLI works and installs in the RIB.

On mgmtd-converted daemons the legacy per-daemon gRPC tree is not the
source of truth mgmtd reads from, so writes through it are a silent
no-op. One more argument that the mgmtd-centred design of #22158 is the
right direction. Details in `results/legacy-1053-split-brain.txt`.

## Reproducing

```
podman build -t frr-22158-test -f mgmtd-grpc-lab/Dockerfile <checkout of e79079be>
# then, inside a privileged container with the source mounted:
bash mgmtd-grpc-lab/run-topotests.sh
```

No vendored code, no patches — everything here is recipe + notes. Happy to
rerun against any revision and report back.

## Consumer context (who is testing this)

For reference, the consumer driving this test effort is a declarative
controller (reconcile loop over desired BGP/policy state) running on
kubernetes with the NAF framework. The mgmtd-centred gRPC endpoint is
exactly the contract it needs:

```
   kubernetes + NAF (our automation, not part of FRR)
   +----------------------------------------------+
   | declarative intent -> observe/diff/apply     |
   +----------------------+-----------------------+
                          | gRPC, frr-northbound.proto
                          v
   +----------------------------------------------+
   | mgmtd (endpoint from PR #22158)              |
   | Get / Commit / Execute / Subscribe           |
   +----+----------------+-------------------+----+
        v                v                   v
   staticd            ripd             (bgpd, someday)
```

Nothing here depends on that stack — the tests and the bench speak plain
gRPC to mgmtd and should be reusable by anyone.
