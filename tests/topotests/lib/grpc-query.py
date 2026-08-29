#!/usr/bin/env python3
# -*- coding: utf-8 eval: (blacken-mode 1) -*-
# SPDX-License-Identifier: MIT
#
# February 22 2022, Christian Hopps <chopps@labn.net>
#
# Copyright (c) 2022, LabN Consulting, L.L.C.

import argparse
import concurrent.futures
import json
import logging
import os
import sys
import tempfile
import threading
import time

import pytest

CWD = os.path.dirname(os.path.realpath(__file__))
TOPOTESTS_DIR = os.path.dirname(CWD)
if TOPOTESTS_DIR not in sys.path:
    sys.path.insert(0, TOPOTESTS_DIR)

tmpdir = None
commander = None
proto_dir = os.path.realpath(os.path.join(CWD, "../../../grpc"))
proto_file = os.path.join(proto_dir, "frr-northbound.proto")

try:
    # Make sure we don't run-into ourselves in parallel operating environment
    tmpdir = tempfile.mkdtemp(prefix="grpc-client-")

    # This is painful but works if you have installed grpc and grpc_tools would be *way*
    # better if we actually built and installed these but ... python packaging.
    try:
        import grpc_tools
        from munet.base import commander

        import grpc

        commander.cmd_raises(
            "python3 -m grpc_tools.protoc"
            f" --python_out={tmpdir} --grpc_python_out={tmpdir}"
            f" -I {proto_dir} {proto_file}"
        )
    except Exception as error:
        logging.error("can't create proto definition modules %s", error)
        raise

    try:
        sys.path[0:0] = [tmpdir]
        import frr_northbound_pb2
        import frr_northbound_pb2_grpc

        sys.path = sys.path[1:]
    except Exception as error:
        logging.error("can't import proto definition modules %s", error)
        raise
finally:
    if commander and tmpdir:
        commander.cmd_nostatus(f"rm -rf {tmpdir}")


class GRPCClient:
    def __init__(self, server, port):
        self.channel = grpc.insecure_channel("{}:{}".format(server, port))
        self.stub = frr_northbound_pb2_grpc.NorthboundStub(self.channel)

    def get_capabilities(self):
        request = frr_northbound_pb2.GetCapabilitiesRequest()
        response = "NONE"
        try:
            response = self.stub.GetCapabilities(request)
        except Exception as error:
            logging.error("Got exception from stub: %s", error)

        logging.debug("GRPC Capabilities: %s", response)
        return response

    def get(self, xpath, encoding, gtype, include_path=False):
        request = frr_northbound_pb2.GetRequest()
        if xpath is not None:
            request.path.append(xpath)
        request.type = gtype
        request.encoding = encoding
        responses = []
        result = ""
        for r in self.stub.Get(request):
            logging.debug('GRPC Get path: "%s" value: %s', request.path, r)
            if include_path:
                responses.append(f"{r.data.path}\n{r.data.data}")
            else:
                result += str(r.data.data)
        if include_path:
            return "\n".join(responses)
        return result

    def get_paths(self, xpaths, encoding, gtype):
        """One Get request with several paths; one response is streamed per path."""
        request = frr_northbound_pb2.GetRequest()
        for xpath in xpaths:
            request.path.append(xpath)
        request.type = gtype
        request.encoding = encoding
        responses = []
        for r in self.stub.Get(request):
            logging.debug('GRPC Get path: "%s" value: %s', request.path, r)
            responses.append(f"{r.data.path}\n{r.data.data}")
        return "\n===RESPONSE===\n".join(responses)

    def execute(self, xpath, input_values):
        request = frr_northbound_pb2.ExecuteRequest()
        request.path = xpath
        for path, value in input_values:
            pv = request.input.add()
            pv.path = path
            pv.value = value
        return self.stub.Execute(request)

    @staticmethod
    def _execute_request(xpath, input_values):
        request = frr_northbound_pb2.ExecuteRequest()
        request.path = xpath
        for path, value in input_values:
            pv = request.input.add()
            pv.path = path
            pv.value = value
        return request

    def execute_cancel(self, xpath, input_values, delay, timeout):
        request = self._execute_request(xpath, input_values)
        future = self.stub.Execute.future(request, timeout=timeout)
        time.sleep(delay)
        future.cancel()

        try:
            future.result()
        except grpc.FutureCancelledError:
            return "CANCELLED"
        except grpc.RpcError as error:
            return error.code().name

        return "OK"

    def execute_concurrent(self, xpath, input_values, count, timeout):
        def run_one():
            request = self._execute_request(xpath, input_values)
            try:
                self.stub.Execute(request, timeout=timeout)
                return "OK"
            except grpc.RpcError as error:
                return error.code().name

        with concurrent.futures.ThreadPoolExecutor(max_workers=count) as executor:
            return json.dumps(list(executor.map(lambda _: run_one(), range(count))))

    def list_transactions(self, timeout):
        request = frr_northbound_pb2.ListTransactionsRequest()
        try:
            ids = [r.id for r in self.stub.ListTransactions(request, timeout=timeout)]
        except grpc.RpcError as error:
            return error.code().name
        return json.dumps(ids)

    def list_transactions_full(self, timeout):
        """List transactions with every field, in streaming order."""
        request = frr_northbound_pb2.ListTransactionsRequest()
        entries = []
        try:
            for r in self.stub.ListTransactions(request, timeout=timeout):
                entries.append(
                    {
                        "id": r.id,
                        "client": r.client,
                        "date": r.date,
                        "comment": r.comment,
                    }
                )
        except grpc.RpcError as error:
            return json.dumps({"error": error.code().name})
        return json.dumps(entries)

    def get_transaction(self, transaction_id, encoding, with_defaults, timeout):
        """Fetch one recorded transaction, reporting refusals inline."""
        request = frr_northbound_pb2.GetTransactionRequest()
        request.transaction_id = transaction_id
        request.encoding = encoding
        request.with_defaults = with_defaults
        try:
            response = self.stub.GetTransaction(request, timeout=timeout)
            return json.dumps(
                {
                    "id": transaction_id,
                    "encoding": response.config.encoding,
                    "config": response.config.data,
                }
            )
        except grpc.RpcError as error:
            return json.dumps(
                {
                    "id": transaction_id,
                    "error": error.code().name,
                    "details": error.details(),
                }
            )

    def commit_result(self, phase_name, updates, deletes):
        """Commit and report the outcome instead of raising on refusal."""
        try:
            response = self.commit_changes(updates, deletes, phase_name)
            return json.dumps(
                {
                    "status": "OK",
                    "transaction_id": response.transaction_id,
                    "error_message": response.error_message,
                }
            )
        except grpc.RpcError as error:
            return json.dumps(
                {"status": error.code().name, "details": error.details()}
            )

    def list_transactions_cancel(self, delay, timeout):
        """Cancel a ListTransactions stream mid-flight.

        Fails the server-side write while the stream is still streaming,
        which is the completion-queue error path that must repost the
        listener for this RPC type.
        """
        request = frr_northbound_pb2.ListTransactionsRequest()
        call = self.stub.ListTransactions(request, timeout=timeout)
        if delay:
            time.sleep(delay)
        call.cancel()

        try:
            list(call)
        except grpc.RpcError as error:
            return error.code().name

        return "OK"

    def list_transactions_hammer(self, count, server, port):
        """Hammer the window where a write fails with the stream in MORE.

        The window is the server's hop to its main thread before it posts
        the write, so a single cancel timing does not reach it: sweep the
        delay and alternate how the call dies (cancel, deadline already
        expired, channel dropped).  Each attempt gets its own channel so a
        dropped one cannot affect the next.  Returns how many attempts ran
        and whether the RPC type still answers afterwards.
        """
        delays = (0, 0.0001, 0.0003, 0.0005, 0.001, 0.002,
                  0.003, 0.005, 0.008, 0.012, 0.02, 0.05)
        deadlines = (0.0002, 0.0005, 0.001, 0.002, 0.005)
        target = "{}:{}".format(server, port)

        for i in range(count):
            channel = grpc.insecure_channel(target)
            stub = frr_northbound_pb2_grpc.NorthboundStub(channel)
            request = frr_northbound_pb2.ListTransactionsRequest()
            mode = i % 3
            try:
                if mode == 1:
                    deadline = deadlines[(i // 3) % len(deadlines)]
                    try:
                        list(stub.ListTransactions(request, timeout=deadline))
                    except grpc.RpcError:
                        pass
                else:
                    delay = delays[(i // 3) % len(delays)]
                    call = stub.ListTransactions(request)
                    if delay:
                        time.sleep(delay)
                    if mode == 0:
                        call.cancel()
                    else:
                        channel.close()
                    try:
                        list(call)
                    except grpc.RpcError:
                        pass
            finally:
                channel.close()

        return json.dumps({"attempts": count})

    def shutdown_hammer(self, threads, seconds, server, port):
        """Keep unary RPCs in flight until the server goes away.

        A unary accepted by the completion-queue thread after the
        daemon's main thread started terminating used to park the gRPC
        pthread in run()'s callback wait forever: the queued callback
        could never run behind the shutdown, so mgmtd never exited
        (#36).  Each hammer thread drives GetCapabilities on its own
        channel until the call fails with the server; an accepted but
        unanswered call is bounded by the per-call timeout.  Returns the
        number of successful calls.
        """
        target = "{}:{}".format(server, port)
        oks = [0] * threads
        stop = time.time() + seconds

        def hammer_one(idx):
            channel = grpc.insecure_channel(target)
            stub = frr_northbound_pb2_grpc.NorthboundStub(channel)
            request = frr_northbound_pb2.GetCapabilitiesRequest()

            while time.time() < stop:
                try:
                    stub.GetCapabilities(request, timeout=seconds)
                except grpc.RpcError:
                    break
                oks[idx] += 1
            channel.close()

        workers = [
            threading.Thread(target=hammer_one, args=(i,), daemon=True)
            for i in range(threads)
        ]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join(timeout=seconds + 15)

        return json.dumps({"oks": sum(oks)})

    # Two grpc.insecure_channel() to one target inside one process share
    # a TCP connection (global subchannel pool), so the server would see
    # ONE channel and one lock owner.  A local pool gives each channel
    # its own connection, which is what "two channels" means server-side.
    LOCAL_POOL = (("grpc.use_local_subchannel_pool", 1),)

    def lock_ownership_scenario(self, server, port, xpath, value_a, value_b):
        """Exercise config-lock ownership across two live channels.

        Channel A takes the lock; channel B must not be able to lock,
        unlock or commit while A holds it; A itself must still be able
        to commit (self-lock); once A unlocks, B must get the lock.
        A and B commit distinct values so the caller can assert the
        running config effect, not just the result codes.
        Returns a JSON dict of the per-step result codes.
        """
        target = "{}:{}".format(server, port)

        def code_of(fn):
            try:
                fn()
                return "OK"
            except grpc.RpcError as error:
                return error.code().name

        def commit_one(stub, value):
            candidate = stub.CreateCandidate(
                frr_northbound_pb2.CreateCandidateRequest()
            )
            edit = frr_northbound_pb2.EditCandidateRequest()
            edit.candidate_id = candidate.candidate_id
            pv = edit.update.add()
            pv.path = xpath
            pv.value = value
            stub.EditCandidate(edit)
            commit = frr_northbound_pb2.CommitRequest()
            commit.candidate_id = candidate.candidate_id
            commit.phase = frr_northbound_pb2.CommitRequest.ALL
            stub.Commit(commit)

        chan_a = grpc.insecure_channel(target, options=self.LOCAL_POOL)
        chan_b = grpc.insecure_channel(target, options=self.LOCAL_POOL)
        stub_a = frr_northbound_pb2_grpc.NorthboundStub(chan_a)
        stub_b = frr_northbound_pb2_grpc.NorthboundStub(chan_b)
        results = {}
        try:
            results["lock_a"] = code_of(
                lambda: stub_a.LockConfig(frr_northbound_pb2.LockConfigRequest())
            )
            results["lock_b_while_a"] = code_of(
                lambda: stub_b.LockConfig(frr_northbound_pb2.LockConfigRequest())
            )
            results["unlock_b_while_a"] = code_of(
                lambda: stub_b.UnlockConfig(frr_northbound_pb2.UnlockConfigRequest())
            )
            results["commit_b_while_a"] = code_of(
                lambda: commit_one(stub_b, value_b)
            )
            results["commit_a_self_lock"] = code_of(
                lambda: commit_one(stub_a, value_a)
            )
            results["unlock_a"] = code_of(
                lambda: stub_a.UnlockConfig(frr_northbound_pb2.UnlockConfigRequest())
            )
            results["lock_b_after"] = code_of(
                lambda: stub_b.LockConfig(frr_northbound_pb2.LockConfigRequest())
            )
            results["unlock_b_after"] = code_of(
                lambda: stub_b.UnlockConfig(frr_northbound_pb2.UnlockConfigRequest())
            )
        finally:
            chan_a.close()
            chan_b.close()
        return json.dumps(results)

    def lock_lease_scenario(self, server, port, retry_secs):
        """Prove the lock lease is the owning channel's life.

        A channel locks and dies without unlocking; a new channel must
        get the lock once the server notices the death (bounded retry).
        Returns the takeover outcome and how long it took.
        """
        target = "{}:{}".format(server, port)

        chan_c = grpc.insecure_channel(target, options=self.LOCAL_POOL)
        stub_c = frr_northbound_pb2_grpc.NorthboundStub(chan_c)
        try:
            stub_c.LockConfig(frr_northbound_pb2.LockConfigRequest())
            lock_c = "OK"
        except grpc.RpcError as error:
            lock_c = error.code().name
        chan_c.close()

        chan_d = grpc.insecure_channel(target, options=self.LOCAL_POOL)
        stub_d = frr_northbound_pb2_grpc.NorthboundStub(chan_d)
        lock_d = "NEVER"
        start = time.time()
        deadline = start + retry_secs
        elapsed = -1.0
        try:
            while time.time() < deadline:
                try:
                    stub_d.LockConfig(frr_northbound_pb2.LockConfigRequest())
                    lock_d = "OK"
                    elapsed = time.time() - start
                    break
                except grpc.RpcError as error:
                    lock_d = error.code().name
                time.sleep(0.25)
            if lock_d == "OK":
                stub_d.UnlockConfig(frr_northbound_pb2.UnlockConfigRequest())
        finally:
            chan_d.close()
        return json.dumps(
            {"lock_c": lock_c, "lock_d": lock_d, "takeover_secs": round(elapsed, 2)}
        )

    def commit_changes(self, updates, deletes, phase_name):
        candidate_id = None

        candidate = self.stub.CreateCandidate(
            frr_northbound_pb2.CreateCandidateRequest()
        )
        candidate_id = candidate.candidate_id

        try:
            edit = frr_northbound_pb2.EditCandidateRequest()
            edit.candidate_id = candidate_id
            for path, value in updates:
                pv = edit.update.add()
                pv.path = path
                pv.value = value
            for path in deletes:
                pv = edit.delete.add()
                pv.path = path
            self.stub.EditCandidate(edit)

            commit = frr_northbound_pb2.CommitRequest()
            commit.candidate_id = candidate_id
            commit.phase = getattr(frr_northbound_pb2.CommitRequest, phase_name)
            return self.stub.Commit(commit)
        finally:
            if candidate_id is not None:
                delete = frr_northbound_pb2.DeleteCandidateRequest()
                delete.candidate_id = candidate_id
                try:
                    self.stub.DeleteCandidate(delete)
                except grpc.RpcError:
                    pass

    def subscribe_listen(self, xpath, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.ON_CHANGE
        request.response_encoding = encoding
        request.path.append(xpath)

        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("update"):
                return response.update.data
        return ""

    def subscribe_listen_with_path(self, xpath, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.ON_CHANGE
        request.response_encoding = encoding
        request.path.append(xpath)

        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("update"):
                return json.dumps(
                    {
                        "path": response.update.path,
                        "data": response.update.data,
                    }
                )
        return ""

    def subscribe_until_sync(self, xpath, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.STREAM
        request.response_encoding = encoding
        request.path.append(xpath)

        responses = []
        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("update"):
                responses.append(
                    {
                        "update": response.update.data,
                        "path": response.update.path,
                    }
                )
            elif response.HasField("sync_response"):
                responses.append({"sync_response": True})
                return json.dumps(responses)
        return json.dumps(responses)

    def subscribe_until_heartbeat(self, xpath, heartbeat_ms, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.ON_CHANGE
        request.response_encoding = encoding
        request.heartbeat_interval_ms = heartbeat_ms
        request.path.append(xpath)

        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("heartbeat"):
                return "heartbeat"
        return ""

    def subscribe_cancel(self, xpath, encoding, delay, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.ON_CHANGE
        request.response_encoding = encoding
        request.path.append(xpath)

        call = self.stub.Subscribe(request, timeout=timeout)
        time.sleep(delay)
        call.cancel()

        try:
            list(call)
        except grpc.RpcError as error:
            return error.code().name

        return "OK"

    def subscribe_expect_shutdown(self, xpath, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.ON_CHANGE
        request.response_encoding = encoding
        request.path.append(xpath)

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            return error.code().name

        return "OK"

    def subscribe_sample_count(
        self, xpath, interval_ms, count, encoding, timeout, snapshot_type=None
    ):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.SAMPLE
        request.response_encoding = encoding
        request.sample_interval_ms = interval_ms
        if snapshot_type is not None:
            request.snapshot_type = getattr(
                frr_northbound_pb2.GetRequest, snapshot_type
            )
        request.path.append(xpath)

        responses = []
        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("update"):
                responses.append(
                    {
                        "path": response.update.path,
                        "data": response.update.data,
                    }
                )
                if len(responses) >= count:
                    return json.dumps(responses)
        return json.dumps(responses)

    def subscribe_expect_error(
        self, mode, xpath, expected, encoding, timeout, snapshot_type=None
    ):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = getattr(frr_northbound_pb2.SubscribeRequest, mode)
        request.response_encoding = encoding
        if xpath:
            request.path.append(xpath)
        if mode == "SAMPLE":
            request.sample_interval_ms = 100
        if snapshot_type is not None:
            request.snapshot_type = getattr(
                frr_northbound_pb2.GetRequest, snapshot_type
            )

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            code = error.code().name
            if code != expected:
                raise AssertionError(f"expected {expected}, got {code}") from error
            return code

        raise AssertionError(f"expected {expected}, got OK")

    def subscribe_invalid_encoding_expect_error(
        self, mode, xpath, bad_encoding, expected, timeout
    ):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = getattr(frr_northbound_pb2.SubscribeRequest, mode)
        request.response_encoding = bad_encoding
        request.path.append(xpath)
        if mode == "SAMPLE":
            request.sample_interval_ms = 100

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            code = error.code().name
            if code != expected:
                raise AssertionError(f"expected {expected}, got {code}") from error
            return code

        raise AssertionError(f"expected {expected}, got OK")

    def subscribe_sample_expect_error(
        self, xpath, interval_ms, expected, encoding, timeout
    ):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.SAMPLE
        request.response_encoding = encoding
        request.sample_interval_ms = interval_ms
        if xpath:
            request.path.append(xpath)

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            code = error.code().name
            if code != expected:
                raise AssertionError(f"expected {expected}, got {code}") from error
            return code

        raise AssertionError(f"expected {expected}, got OK")

    def subscribe_stream_repeat_expect_error(
        self, xpath, repeat, expected, encoding, timeout
    ):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.STREAM
        request.response_encoding = encoding
        for _ in range(repeat):
            request.path.append(xpath)

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            code = error.code().name
            if code != expected:
                raise AssertionError(f"expected {expected}, got {code}") from error
            return code

        raise AssertionError(f"expected {expected}, got OK")

    def subscribe_sample_cancel(self, xpath, interval_ms, delay, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.SAMPLE
        request.response_encoding = encoding
        request.sample_interval_ms = interval_ms
        request.path.append(xpath)

        call = self.stub.Subscribe(request, timeout=timeout)
        time.sleep(delay)
        call.cancel()

        try:
            list(call)
        except grpc.RpcError as error:
            return error.code().name

        return "OK"

    def subscribe_stream_paths_order(self, xpaths, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.STREAM
        request.response_encoding = encoding
        for xpath in xpaths:
            request.path.append(xpath)

        events = []
        synced = False
        for response in self.stub.Subscribe(request, timeout=timeout):
            if response.HasField("update"):
                events.append(
                    {
                        "update": response.update.path,
                        "empty": not response.update.data,
                    }
                )
                if synced:
                    return json.dumps(events)
            elif response.HasField("sync_response"):
                synced = True
                events.append({"sync_response": True})
        return json.dumps(events)

    def subscribe_stream_paths_expect_error(self, xpaths, expected, encoding, timeout):
        request = frr_northbound_pb2.SubscribeRequest()
        request.mode = frr_northbound_pb2.SubscribeRequest.STREAM
        request.response_encoding = encoding
        for xpath in xpaths:
            request.path.append(xpath)

        try:
            list(self.stub.Subscribe(request, timeout=timeout))
        except grpc.RpcError as error:
            code = error.code().name
            if code != expected:
                raise AssertionError(f"expected {expected}, got {code}") from error
            return code

        raise AssertionError(f"expected {expected}, got OK")


def next_action(action_list=None):
    "Get next action from list or STDIN"
    if action_list:
        for action in action_list:
            yield action
    else:
        while True:
            try:
                action = input("")
                if not action:
                    break
                yield action.strip()
            except EOFError:
                break


def main(*args):
    parser = argparse.ArgumentParser(description="gRPC Client")
    parser.add_argument(
        "-s", "--server", default="localhost", help="gRPC Server Address"
    )
    parser.add_argument(
        "-p", "--port", type=int, default=50051, help="gRPC Server TCP Port"
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="be verbose")
    parser.add_argument("--check", action="store_true", help="check runable")
    parser.add_argument("--xml", action="store_true", help="encode XML instead of JSON")
    parser.add_argument("actions", nargs="*", help="GETCAP|GET,xpath")
    args = parser.parse_args(*args)

    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s %(levelname)s: GRPC-CLI-CLIENT: %(name)s %(message)s",
    )

    if args.check:
        sys.exit(0)

    encoding = frr_northbound_pb2.XML if args.xml else frr_northbound_pb2.JSON

    c = GRPCClient(args.server, args.port)

    for action in next_action(args.actions):
        raw_action = action
        action = action.casefold()
        logging.debug("GOT ACTION: %s", action)
        if action == "getcap":
            caps = c.get_capabilities()
            print(caps)
        elif action == "get-config":
            print(
                c.get(
                    None,
                    encoding,
                    gtype=frr_northbound_pb2.GetRequest.CONFIG,
                )
            )
        elif action.startswith("get,"):
            # Get and print config and state
            _, xpath = action.split(",", 1)
            logging.debug("Get XPath: %s", xpath)
            print(c.get(xpath, encoding, gtype=frr_northbound_pb2.GetRequest.ALL))
        elif action.startswith("get-config,"):
            # Get and print config
            _, xpath = action.split(",", 1)
            logging.debug("Get Config XPath: %s", xpath)
            print(c.get(xpath, encoding, gtype=frr_northbound_pb2.GetRequest.CONFIG))
            # for _ in range(0, 1):
        elif action.startswith("get-config-with-path,"):
            _, xpath = action.split(",", 1)
            logging.debug("Get Config XPath: %s", xpath)
            print(
                c.get(
                    xpath,
                    encoding,
                    gtype=frr_northbound_pb2.GetRequest.CONFIG,
                    include_path=True,
                )
            )
        elif action.startswith("get-state,"):
            # Get and print state
            _, xpath = action.split(",", 1)
            logging.debug("Get State XPath: %s", xpath)
            print(c.get(xpath, encoding, gtype=frr_northbound_pb2.GetRequest.STATE))
            # for _ in range(0, 1):
        elif action.startswith("get-state-paths,"):
            # Get and print state for several paths in one request
            _, xpaths = action.split(",", 1)
            logging.debug("Get State XPaths: %s", xpaths)
            print(
                c.get_paths(
                    xpaths.split(";"),
                    encoding,
                    gtype=frr_northbound_pb2.GetRequest.STATE,
                )
            )
        elif action.startswith("exec,"):
            # Execute an RPC. Input arguments are path=value pairs.
            parts = raw_action.split(",")
            xpath = parts[1]
            input_values = []
            for item in parts[2:]:
                path, value = item.split("=", 1)
                input_values.append((path, value))
            response = c.execute(xpath, input_values)
            print(response)
        elif action.startswith("exec-cancel,"):
            parts = raw_action.split(",")
            xpath = parts[1]
            delay = float(parts[2])
            timeout = float(parts[3])
            input_values = []
            for item in parts[4:]:
                path, value = item.split("=", 1)
                input_values.append((path, value))
            print(c.execute_cancel(xpath, input_values, delay, timeout))
        elif action.startswith("exec-concurrent,"):
            parts = raw_action.split(",")
            xpath = parts[1]
            count = int(parts[2])
            timeout = float(parts[3])
            input_values = []
            for item in parts[4:]:
                path, value = item.split("=", 1)
                input_values.append((path, value))
            print(c.execute_concurrent(xpath, input_values, count, timeout))
        elif action.startswith("list-transactions-hammer,"):
            _, count = raw_action.split(",", 1)
            print(c.list_transactions_hammer(int(count), args.server, args.port))
        elif action.startswith("shutdown-hammer,"):
            _, threads, seconds = raw_action.split(",")
            print(c.shutdown_hammer(int(threads), int(seconds), args.server, args.port))
        elif action.startswith("lock-ownership-scenario,"):
            _, xpath, value_a, value_b = raw_action.split(",", 3)
            print(
                c.lock_ownership_scenario(
                    args.server, args.port, xpath, value_a, value_b
                )
            )
        elif action.startswith("lock-lease-scenario,"):
            _, retry_secs = raw_action.split(",", 1)
            print(c.lock_lease_scenario(args.server, args.port, float(retry_secs)))
        elif action.startswith("list-transactions-cancel,"):
            _, delay, timeout = raw_action.split(",", 2)
            print(c.list_transactions_cancel(float(delay), float(timeout)))
        elif action.startswith("list-transactions,"):
            _, timeout = raw_action.split(",", 1)
            print(c.list_transactions(float(timeout)))
        elif action.startswith("list-transactions-full,"):
            _, timeout = raw_action.split(",", 1)
            print(c.list_transactions_full(float(timeout)))
        elif action.startswith("get-transaction,"):
            parts = raw_action.split(",")
            print(
                c.get_transaction(
                    int(parts[1]), int(parts[2]), bool(int(parts[3])), 30.0
                )
            )
        elif action.startswith("commit-result,"):
            parts = raw_action.split(",")
            phase = parts[1]
            updates = []
            deletes = []
            for item in parts[2:]:
                if "=" in item:
                    path, value = item.rsplit("=", 1)
                    updates.append((path, value))
                else:
                    deletes.append(item)
            print(c.commit_result(phase, updates, deletes))
        elif action.startswith("commit-set,"):
            parts = raw_action.split(",")
            updates = []
            for item in parts[1:]:
                path, value = item.rsplit("=", 1)
                updates.append((path, value))
            print(c.commit_changes(updates, [], "ALL"))
        elif action.startswith("commit-delete,"):
            parts = raw_action.split(",")
            print(c.commit_changes([], parts[1:], "ALL"))
        elif action.startswith("commit-phase,"):
            parts = raw_action.split(",")
            phase = parts[1]
            updates = []
            deletes = []
            for item in parts[2:]:
                if "=" in item:
                    path, value = item.rsplit("=", 1)
                    updates.append((path, value))
                else:
                    deletes.append(item)
            print(c.commit_changes(updates, deletes, phase))
        elif action.startswith("subscribe-listen,"):
            _, xpath, timeout = raw_action.split(",", 2)
            print(c.subscribe_listen(xpath, encoding, float(timeout)))
        elif action.startswith("subscribe-listen-with-path,"):
            _, xpath, timeout = raw_action.split(",", 2)
            print(c.subscribe_listen_with_path(xpath, encoding, float(timeout)))
        elif action.startswith("subscribe-until-sync,"):
            _, xpath, timeout = raw_action.split(",", 2)
            print(c.subscribe_until_sync(xpath, encoding, float(timeout)))
        elif action.startswith("subscribe-until-heartbeat,"):
            _, xpath, heartbeat_ms, timeout = raw_action.split(",", 3)
            print(
                c.subscribe_until_heartbeat(
                    xpath, int(heartbeat_ms), encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-cancel,"):
            _, xpath, delay, timeout = raw_action.split(",", 3)
            print(c.subscribe_cancel(xpath, encoding, float(delay), float(timeout)))
        elif action.startswith("subscribe-expect-shutdown,"):
            _, xpath, timeout = raw_action.split(",", 2)
            print(c.subscribe_expect_shutdown(xpath, encoding, float(timeout)))
        elif action.startswith("subscribe-sample-count,"):
            _, xpath, interval_ms, count, timeout = raw_action.split(",", 4)
            print(
                c.subscribe_sample_count(
                    xpath, int(interval_ms), int(count), encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-sample-count-typed,"):
            _, xpath, interval_ms, count, snapshot_type, timeout = raw_action.split(
                ",", 5
            )
            print(
                c.subscribe_sample_count(
                    xpath,
                    int(interval_ms),
                    int(count),
                    encoding,
                    float(timeout),
                    snapshot_type=snapshot_type,
                )
            )
        elif action.startswith("subscribe-expect-error,"):
            _, mode, xpath, expected, timeout = raw_action.split(",", 4)
            print(
                c.subscribe_expect_error(
                    mode, xpath, expected, encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-typed-expect-error,"):
            _, mode, xpath, snapshot_type, expected, timeout = raw_action.split(",", 5)
            print(
                c.subscribe_expect_error(
                    mode,
                    xpath,
                    expected,
                    encoding,
                    float(timeout),
                    snapshot_type=snapshot_type,
                )
            )
        elif action.startswith("subscribe-sample-expect-error,"):
            _, xpath, interval_ms, expected, timeout = raw_action.split(",", 4)
            print(
                c.subscribe_sample_expect_error(
                    xpath, int(interval_ms), expected, encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-invalid-encoding-expect-error,"):
            _, mode, xpath, bad_encoding, expected, timeout = raw_action.split(",", 5)
            print(
                c.subscribe_invalid_encoding_expect_error(
                    mode, xpath, int(bad_encoding), expected, float(timeout)
                )
            )
        elif action.startswith("subscribe-stream-repeat-expect-error,"):
            _, xpath, repeat, expected, timeout = raw_action.split(",", 4)
            print(
                c.subscribe_stream_repeat_expect_error(
                    xpath, int(repeat), expected, encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-sample-cancel,"):
            _, xpath, interval_ms, delay, timeout = raw_action.split(",", 4)
            print(
                c.subscribe_sample_cancel(
                    xpath, int(interval_ms), float(delay), encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-stream-paths-order,"):
            _, xpaths, timeout = raw_action.split(",", 2)
            print(
                c.subscribe_stream_paths_order(
                    xpaths.split(";"), encoding, float(timeout)
                )
            )
        elif action.startswith("subscribe-stream-paths-expect-error,"):
            _, xpaths, expected, timeout = raw_action.split(",", 3)
            print(
                c.subscribe_stream_paths_expect_error(
                    xpaths.split(";"), expected, encoding, float(timeout)
                )
            )


if __name__ == "__main__":
    main()
