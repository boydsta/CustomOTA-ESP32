#!/usr/bin/env python3
"""
CustomOTA device test - drives a running device's OTA endpoints over HTTP.

Standard library only (Python 3.8+). Not part of the published library.

    python tools/ota_test.py --host 192.168.1.50 --firmware path/to/firmware.bin
    python tools/ota_test.py --host 192.168.1.50 --firmware fw.bin --user admin --password secret
    python tools/ota_test.py --list

--firmware must be a CustomOTA build for this device (it is installed during the test).
--upload-timeout must match the device's setUploadTimeout() (default 30 s).

What happens to the device
  * Failure tests (garbage, truncated, corrupt, stalled, dropped, concurrent uploads)
    only ever send a corrupted image or random bytes. ESP-IDF verifies the whole image
    before it switches the boot partition, so none of them can boot. They do overwrite
    the spare OTA slot - the running firmware is never touched.
  * Restart tests install --firmware into the spare slot, reboot, then roll back.
  * The run ends with a restore step that rolls back to the partition the device was
    running when the test started, so it ends on your original firmware.
  * Never applies a partition table. Only uploads one and cancels it.
  * --fs replaces the SPIFFS contents with the image you give it. Off unless set.
  * --gate is interactive: it asks you to block and unblock updates in your application
    (setUpdateAllowed) and checks the device refuses everything while blocked.

Exit code is 1 if any test failed.
"""

import argparse
import base64
import http.client
import json
import random
import re
import socket
import sys
import time

ESP_IMAGE_MAGIC = 0xE9
PARTITION_TABLE_MAX_LEN = 0xC00   # partitions.bin as PlatformIO builds it
PARTITION_TABLE_SIZE = 0x1000     # the flash sector holding it - the device's upload limit

RESULT_COLOURS = {"PASS": "\033[32m", "FAIL": "\033[31m", "WARN": "\033[33m", "SKIP": "\033[90m"}


class Fail(Exception):
    pass


class Skip(Exception):
    pass


class Warn(Exception):
    pass


def check(cond, msg):
    if not cond:
        raise Fail(msg)


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------

class Device:
    def __init__(self, host, port, user, password, timeout=10):
        self.host = host
        self.port = port
        self.creds = (user, password) if user else None
        self.timeout = timeout

    def auth_header(self, auth):
        """auth: True = configured credentials, False = none, (user, pw) = these."""
        creds = self.creds if auth is True else (auth or None)
        if not creds:
            return None
        return "Basic " + base64.b64encode(f"{creds[0]}:{creds[1]}".encode()).decode()

    def request(self, method, path, body=None, headers=None, auth=True, timeout=None):
        conn = http.client.HTTPConnection(self.host, self.port, timeout=timeout or self.timeout)
        h = {"Connection": "close"}
        if headers:
            h.update(headers)
        a = self.auth_header(auth)
        if a:
            h["Authorization"] = a
        if body is None and method == "POST":
            body = b""
        try:
            conn.request(method, path, body=body, headers=h)
            r = conn.getresponse()
            data = r.read()
            return r.status, {k.lower(): v for k, v in r.getheaders()}, data
        finally:
            conn.close()

    def status(self, timeout=None):
        code, _, data = self.request("GET", "/update/status", timeout=timeout)
        if code != 200:
            raise Fail(f"/update/status returned {code}")
        try:
            return json.loads(data)
        except ValueError:
            raise Fail(f"/update/status is not valid JSON: {data[:300]!r}")

    def post(self, path, auth=True):
        code, _, data = self.request("POST", path, auth=auth)
        return code, data.decode(errors="replace")


def multipart(data, kind, filename, boundary):
    """The type field goes first so the device knows it when the file starts."""
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"type\"\r\n\r\n{kind}\r\n").encode()
    if data is not None:
        body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; "
                 f"filename=\"{filename}\"\r\nContent-Type: application/octet-stream\r\n\r\n").encode()
        body += data + b"\r\n"
    body += f"--{boundary}--\r\n".encode()
    return body


class Upload:
    """Multipart upload over a raw socket, so a test controls exactly when data stops."""

    def __init__(self, dev, data, kind="firmware", auth=True, filename=None, path="/update"):
        boundary = "cotatest%016x" % random.getrandbits(64)
        if filename is None:
            filename = "spiffs.bin" if kind == "filesystem" else "firmware.bin"
        self.body = multipart(data, kind, filename, boundary)
        self.sent = 0
        head = [f"POST {path}?type={kind} HTTP/1.1",
                f"Host: {dev.host}",
                f"Content-Type: multipart/form-data; boundary={boundary}",
                f"Content-Length: {len(self.body)}",
                "Connection: close"]
        a = dev.auth_header(auth)
        if a:
            head.append("Authorization: " + a)
        self.sock = socket.create_connection((dev.host, dev.port), timeout=60)
        self.sock.sendall(("\r\n".join(head) + "\r\n\r\n").encode())

    def send_to(self, fraction):
        self._send(int(len(self.body) * fraction))

    def send_rest(self):
        self._send(len(self.body))

    def _send(self, end):
        while self.sent < end:
            n = min(4096, end - self.sent)
            self.sock.sendall(self.body[self.sent:self.sent + n])
            self.sent += n

    def response(self, timeout=180):
        """Returns (status code, body text)."""
        self.sock.settimeout(timeout)
        buf = b""
        try:
            while True:
                try:
                    chunk = self.sock.recv(4096)
                except socket.timeout:
                    raise Fail(f"no response from the device within {timeout} s")
                if not chunk:
                    break
                buf += chunk
                head, sep, rest = buf.partition(b"\r\n\r\n")
                if sep:
                    m = re.search(rb"content-length:\s*(\d+)", head, re.I)
                    if m and len(rest) >= int(m.group(1)):
                        break
        finally:
            self.close()
        if not buf:
            raise Fail("device closed the connection without a response")
        parts = buf.split(b"\r\n", 1)[0].split()
        code = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
        return code, buf.partition(b"\r\n\r\n")[2].decode(errors="replace")

    def finish(self, timeout=180):
        self.send_rest()
        return self.response(timeout)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

# Run order. kind: safe = no restart, restart = restarts the device, fs/gate = opt-in.
# Each name runs Tester.t_<name>.
TESTS = [
    ("status", "safe", "status JSON parses and has the 1.6 fields"),
    ("page", "safe", "OTA page is served, not cached, and is the 1.6 page"),
    ("auth", "safe", "every endpoint refuses missing or wrong credentials"),
    ("partitions", "safe", "partition editor rejects bad files; upload + cancel (--partitions)"),
    ("no_file", "safe", "upload without a file part gets 400"),
    ("garbage", "safe", "non-image file is refused, running firmware unchanged"),
    ("truncated", "safe", "half an image is refused at the end"),
    ("corrupt", "safe", "image with a damaged middle is refused at the end"),
    ("rollback_refused", "safe", "rollback to a damaged spare slot is refused"),
    ("stall", "safe", "upload that stops sending is aborted after the timeout"),
    ("stall_late_data", "safe", "the stalled upload then sends the rest: refused with 400"),
    ("drop", "safe", "upload whose connection closes is aborted after the timeout"),
    ("unauth_during_upload", "safe", "unauthenticated request can't disturb an upload"),
    ("heap", "safe", "repeated failed uploads don't leak heap"),
    ("filesystem", "fs", "filesystem image upload (--fs, replaces SPIFFS contents)"),
    ("gate", "gate", "setUpdateAllowed(false) blocks everything (--gate, interactive)"),
    ("upload_ok", "restart", "good firmware installs and boots; upload during restart refused"),
    ("reboot", "restart", "reboot endpoint restarts on the same partition"),
    ("rollback", "restart", "rollback boots the previous firmware"),
    ("concurrent", "restart", "second upload during an upload doesn't break the first"),
    ("soak", "restart", "install + rollback repeated --soak times"),
]


class Tester:
    def __init__(self, args):
        self.args = args
        self.dev = Device(args.host, args.port, args.user, args.password)
        self.results = []
        self.timeout = args.upload_timeout
        self.auth_required = False
        self.partitions_enabled = None
        self.original = None          # status when the run started
        self.backup_damaged = False   # a failed upload has overwritten the spare slot
        self.restarted = False        # a restart test ran, so restore at the end
        self.stalled_upload = None    # (Upload, status before) left open by the stall test

        with open(args.firmware, "rb") as f:
            self.fw = f.read()
        if len(self.fw) < 65536 or self.fw[0] != ESP_IMAGE_MAGIC:
            sys.exit(f"{args.firmware} is not an ESP32 app image (first byte must be 0xE9)")

        rnd = random.Random(1)
        # Valid header, damaged middle: accepted while uploading, refused by the image check
        mid = len(self.fw) // 2
        self.corrupt = self.fw[:mid] + bytes(b ^ 0xFF for b in self.fw[mid:mid + 512]) + self.fw[mid + 512:]
        # Damaged early (inside the first 30%), for the concurrent test: if the device splices
        # this upload's start onto the good image's remainder, the result fails the image check
        early = len(self.fw) // 10
        self.corrupt_early = (self.fw[:early] + bytes(b ^ 0xFF for b in self.fw[early:early + 512])
                              + self.fw[early + 512:])
        self.truncated = self.fw[:len(self.fw) // 2]
        # Wrong first byte: refused at the first flash write, before anything is erased
        self.garbage = b"\x00" + bytes(rnd.getrandbits(8) for _ in range(65535))

        self.fs = None
        if args.fs:
            with open(args.fs, "rb") as f:
                self.fs = f.read()
        self.ptbin = None
        if args.partitions:
            with open(args.partitions, "rb") as f:
                self.ptbin = f.read()

        self.tests = [(name, kind, getattr(self, "t_" + name), desc) for name, kind, desc in TESTS]

    # --- bookkeeping --------------------------------------------------------

    def record(self, name, result, detail=""):
        self.results.append((name, result, detail))
        colour = RESULT_COLOURS.get(result, "") if sys.stdout.isatty() else ""
        reset = "\033[0m" if colour else ""
        print(f"{colour}[{result}]{reset} {name}" + (f" - {detail}" if detail else ""), flush=True)

    def run(self, name, fn):
        try:
            detail = fn()
            self.record(name, "PASS", detail or "")
        except Skip as e:
            self.record(name, "SKIP", str(e))
        except Warn as e:
            self.record(name, "WARN", str(e))
        except Fail as e:
            self.record(name, "FAIL", str(e))
        except (OSError, http.client.HTTPException) as e:
            self.record(name, "FAIL", f"connection error: {e!r}")

    def info(self, msg):
        print(f"       {msg}", flush=True)

    # --- device state helpers -----------------------------------------------

    def wait_online(self, timeout=60):
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                return self.dev.status(timeout=3)
            except (OSError, http.client.HTTPException):
                time.sleep(1)
        raise Fail(f"device not responding after {timeout} s")

    def idle(self):
        """Wait for any upload or restart left over from an earlier test to clear."""
        limit = self.timeout + 30
        t0 = time.time()
        while True:
            s = self.wait_online()
            if not s["inProgress"] and not s["restartPending"] and s.get("backupChecked", True):
                return s
            if time.time() - t0 > limit:
                raise Fail(f"device still busy after {limit} s: {s['status']}")
            time.sleep(1)

    def wait_until(self, cond, timeout):
        """Poll status until cond(s). A slow or dropped status request is retried."""
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                s = self.dev.status(timeout=5)
            except (OSError, http.client.HTTPException):
                time.sleep(0.5)
                continue
            if cond(s):
                return s, time.time() - t0
            time.sleep(0.5)
        raise Fail(f"condition not reached within {timeout} s")

    def wait_restart(self, timeout=90, expect_partition=None, settle=True):
        """Wait for the device to go down and come back. With expect_partition, also done
        as soon as it reports running that partition (in case the restart was missed).
        With settle, then waits for the background backup check (1.6+) to finish."""
        t0 = time.time()
        down = False
        s = None
        while time.time() - t0 < timeout:
            try:
                s = self.dev.status(timeout=2)
                if down:
                    break
                if (expect_partition and s["runningPartition"] == expect_partition
                        and not s["restartPending"]):
                    break
                s = None
            except (OSError, http.client.HTTPException):
                down = True
            time.sleep(0.5)
        if s is None:
            raise Fail(f"device did not restart and come back within {timeout} s")
        if settle and not s.get("backupChecked", True):
            s, _ = self.wait_until(lambda st: st.get("backupChecked", True), 60)
        return s

    def unchanged(self, before, after):
        check(after["runningPartition"] == before["runningPartition"],
              f"running partition changed from {before['runningPartition']} to {after['runningPartition']}")
        check(not after["restartPending"], "a restart is pending")
        check(not after["inProgress"], "upload still marked in progress")

    def expect_refused(self, data, code_ok, text_hint=None, auth=True):
        """Upload data, expect one of code_ok, and the device left as it was."""
        before = self.idle()
        code, text = Upload(self.dev, data, auth=auth).finish()
        check(code in code_ok, f"expected {code_ok}, got {code}: {text}")
        if text_hint:
            check(text_hint.lower() in text.lower(), f"expected '{text_hint}' in the reply, got: {text}")
        after = self.dev.status()
        self.unchanged(before, after)
        return code, text, after

    def rollback_to(self, partition):
        s = self.idle()
        if s["runningPartition"] == partition:
            return s
        code, text = self.dev.post("/update/rollback")
        check(code == 200, f"rollback returned {code}: {text}")
        s = self.wait_restart(expect_partition=partition)
        check(s["runningPartition"] == partition,
              f"after rollback running {s['runningPartition']}, expected {partition}")
        return s

    # --- safe tests ---------------------------------------------------------

    def t_status(self):
        s = self.dev.status()
        needed = ["inProgress", "lastUploadFailed", "progress", "status", "hasBackup", "freeHeap",
                  "runningPartition", "backupPartition", "canRollback", "restartPending",
                  "updateAllowed", "blockedReason", "libraryVersion"]
        missing = [k for k in needed if k not in s]
        check(not missing, f"missing fields {missing} - device runs CustomOTA {s.get('libraryVersion', '?')}")
        return (f"CustomOTA {s['libraryVersion']}, app '{s.get('version', '')}', running "
                f"{s['runningPartition']}, spare {s['backupPartition'] or '-'}, heap {s['freeHeap']}")

    def t_page(self):
        code, headers, body = self.dev.request("GET", "/update")
        check(code == 200, f"GET /update returned {code}")
        check(b"<html" in body.lower(), "reply is not HTML")
        check("no-store" in headers.get("cache-control", ""), "page is missing Cache-Control: no-store")
        check(b"lastUploadFailed" in body, "page has no upload-state code - not the 1.6 page")
        return f"{len(body)} bytes"

    def t_auth(self):
        if not self.auth_required:
            raise Skip("device has no credentials set")
        before = self.idle()
        wrong = (self.args.user, self.args.password + "x")
        for auth, label in ((False, "no credentials"), (wrong, "wrong password")):
            code, _, _ = self.dev.request("GET", "/update", auth=auth)
            check(code == 401, f"GET /update with {label} returned {code}")
            for path in ("/update/reboot", "/update/rollback"):
                code, text = self.dev.post(path, auth=auth)
                check(code == 401, f"POST {path} with {label} returned {code}: {text}")
            if self.partitions_enabled:
                code, _, _ = self.dev.request("GET", "/update/partitions", auth=auth)
                check(code == 401, f"GET /update/partitions with {label} returned {code}")
                for path in ("/update/partitions/apply", "/update/partitions/cancel"):
                    code, text = self.dev.post(path, auth=auth)
                    check(code == 401, f"POST {path} with {label} returned {code}: {text}")
            code, text = Upload(self.dev, self.corrupt, auth=auth).finish()
            check(code == 401, f"upload with {label} returned {code}: {text}")
        after = self.dev.status()
        self.unchanged(before, after)
        return "all endpoints answer 401 (status JSON is open by design)"

    def t_partitions(self):
        if not self.partitions_enabled:
            raise Skip("device built without CUSTOM_OTA_PARTITIONS")
        self.dev.post("/update/partitions/cancel")
        notes = []

        def upload(data, filename="partitions.bin"):
            return Upload(self.dev, data, path="/update/partitions", filename=filename).finish(30)

        code, text = upload(bytes(random.getrandbits(8) for _ in range(PARTITION_TABLE_MAX_LEN)))
        check(code == 400, f"random bytes as a partition table returned {code}: {text}")
        code, text = upload(self.fw[:PARTITION_TABLE_SIZE + 1], "firmware.bin")
        check(code == 400 and "too big" in text, f"oversized file returned {code}: {text}")
        code, text = self.dev.post("/update/partitions/apply")
        check(code == 400, f"apply with nothing uploaded returned {code}: {text}")
        notes.append("bad files refused, apply without upload refused")

        if self.ptbin is None:
            notes.append("upload + cancel not run (no --partitions)")
            return "; ".join(notes)
        try:
            code, text = upload(self.ptbin)
            check(code == 200, f"partitions.bin upload returned {code}: {text}")
            info = json.loads(text)
            check(info.get("pending"), "upload accepted but nothing pending")
            if info.get("errors"):
                self.info(f"device reports errors for this table: {info['errors']}")
            if info.get("warnings"):
                self.info(f"device warnings: {info['warnings']}")
        finally:
            code, text = self.dev.post("/update/partitions/cancel")
        check(code == 200, f"cancel returned {code}: {text}")
        code, _, data = self.dev.request("GET", "/update/partitions")
        check(code == 200 and json.loads(data).get("pending") is None, "table still pending after cancel")
        notes.append("upload accepted and cancelled, table unchanged")
        return "; ".join(notes)

    def t_no_file(self):
        _, text, after = self.expect_refused(None, (400,))
        check(after["lastUploadFailed"], "lastUploadFailed not set")
        return text

    def t_garbage(self):
        before = self.idle()
        _, text, after = self.expect_refused(self.garbage, (500,))
        check(after["lastUploadFailed"], "lastUploadFailed not set")
        # Refused at its first block, before anything was erased: the backup is still there
        check(after["hasBackup"] == before["hasBackup"],
              f"hasBackup changed from {before['hasBackup']} to {after['hasBackup']} though nothing was written")
        return text

    def t_truncated(self):
        _, text, after = self.expect_refused(self.truncated, (500,))
        self.backup_damaged = True
        check(after["lastUploadFailed"], "lastUploadFailed not set")
        check(not after["hasBackup"], "hasBackup still true after a failed upload overwrote the spare slot")
        return text

    def t_corrupt(self):
        _, text, after = self.expect_refused(self.corrupt, (500,))
        self.backup_damaged = True
        check(after["lastUploadFailed"], "lastUploadFailed not set")
        check(not after["hasBackup"], "hasBackup still true after a failed upload overwrote the spare slot")
        return text

    def t_rollback_refused(self):
        if not self.backup_damaged:
            raise Skip("needs the truncated or corrupt test first, so the spare slot holds a bad image")
        before = self.idle()
        if not before["canRollback"]:
            raise Skip("device has no second OTA slot")
        code, text = self.dev.post("/update/rollback")
        if code == 200:
            self.wait_restart()
            raise Fail("rollback accepted a damaged image and restarted")
        check(code == 400, f"rollback returned {code}: {text}")
        self.unchanged(before, self.dev.status())
        check(not before["hasBackup"], "refused correctly, but status still reports hasBackup=true after a "
              "failed upload overwrote the spare slot, so the page offers a rollback that will fail")
        return text

    def _start_partial(self, data, fraction):
        """Start an upload, send part of it, and check the device reports it."""
        u = Upload(self.dev, data)
        u.send_to(fraction)
        time.sleep(1.5)
        s = self.dev.status()
        check(s["inProgress"], f"upload not reported in progress: {s['status']}")
        check(0 < s["progress"] < 100, f"progress is {s['progress']}")
        check(not s["lastUploadFailed"], "lastUploadFailed not cleared when the upload started")
        return u, s

    def _wait_stall_abort(self, before, sent_at):
        s, _ = self.wait_until(lambda s: not s["inProgress"], self.timeout + 30)
        elapsed = time.time() - sent_at
        check(elapsed >= self.timeout - 3,
              f"aborted after {elapsed:.0f} s, earlier than --upload-timeout {self.timeout} s")
        check(s["lastUploadFailed"], "lastUploadFailed not set")
        check("stall" in s["status"].lower(), f"status does not mention the stall: {s['status']}")
        self.unchanged(before, s)
        return s, elapsed

    def t_stall(self):
        before = self.idle()
        u, s = self._start_partial(self.corrupt, 0.4)
        sent_at = time.time() - 1.5
        self.info(f"upload paused at {s['progress']}%, waiting for the {self.timeout} s timeout...")
        try:
            s, elapsed = self._wait_stall_abort(before, sent_at)
        except BaseException:
            u.close()
            raise
        self.stalled_upload = (u, before)
        return f"aborted after {elapsed:.0f} s: {s['status']}"

    def t_stall_late_data(self):
        """The stalled connection comes back and sends the rest: it must not be applied."""
        if not self.stalled_upload:
            raise Skip("needs the stall test first")
        u, before = self.stalled_upload
        self.stalled_upload = None
        try:
            code, text = u.finish(30)
        except (OSError, Fail):
            # ESPAsyncWebServer closes a connection that sends nothing for 3 s mid-body,
            # so late data can't arrive on it - which is also a safe outcome
            self.unchanged(before, self.dev.status())
            return "web server had closed the idle connection, so late data can't be applied"
        check(code == 400, f"late data after the abort returned {code}: {text}")
        self.unchanged(before, self.dev.status())
        return f"{code}: {text}"

    def t_drop(self):
        before = self.idle()
        u, s = self._start_partial(self.corrupt, 0.4)
        sent_at = time.time() - 1.5
        u.close()
        self.info(f"connection closed at {s['progress']}%, waiting for the {self.timeout} s timeout...")
        s, elapsed = self._wait_stall_abort(before, sent_at)
        return f"aborted after {elapsed:.0f} s: {s['status']}"

    def t_unauth_during_upload(self):
        if not self.auth_required:
            raise Skip("device has no credentials set")
        before = self.idle()
        # Corrupt image: an intact stream ends with 500 (image check), a disturbed one with 400
        u, s = self._start_partial(self.corrupt, 0.3)
        try:
            code, text = Upload(self.dev, self.garbage, auth=False).finish(30)
            check(code == 401, f"unauthenticated upload returned {code}: {text}")
            mid = self.dev.status()
            code, text = u.finish()
        finally:
            u.close()
        self.unchanged(before, self.dev.status())
        check(mid["inProgress"], "an unauthenticated upload request ended the upload in progress")
        check(code == 500, f"first upload should end with 500 (image check), got {code}: {text}")
        return "upload continued undisturbed"

    def t_heap(self):
        self.idle()
        heap_before = max(self.dev.status()["freeHeap"] for _ in range(3))
        rounds = 10
        for _ in range(rounds):
            code, _ = Upload(self.dev, self.garbage).finish(60)
            check(code == 500, f"garbage upload returned {code}")
            code, _ = Upload(self.dev, None).finish(30)
            check(code == 400, f"no-file upload returned {code}")
            self.dev.status()
        time.sleep(1)
        heap_after = max(self.dev.status()["freeHeap"] for _ in range(3))
        lost = heap_before - heap_after
        detail = f"{rounds * 2} failed uploads, free heap {heap_before} -> {heap_after} ({-lost:+d} bytes)"
        if lost > 4096:
            raise Warn(detail)
        return detail

    def t_filesystem(self):
        if self.fs is None:
            raise Skip("no --fs image given")
        before = self.idle()
        code, text = Upload(self.dev, self.fs, kind="filesystem").finish()
        check(code == 200, f"filesystem upload returned {code}: {text}")
        after = self.dev.status()
        self.unchanged(before, after)
        check(not after["lastUploadFailed"], "lastUploadFailed set after a good upload")
        check(after["spiffsMounted"], "SPIFFS not mounted after the update")
        return text

    def t_gate(self):
        if not self.args.gate:
            raise Skip("interactive - run with --gate")
        before = self.idle()
        if not before["updateAllowed"]:
            input("Updates are blocked on the device. Allow them in your application, then press Enter...")
            before, _ = self.wait_until(lambda s: s["updateAllowed"], 120)

        u, _ = self._start_partial(self.corrupt, 0.3)
        try:
            input(f"Upload running. Block updates in your application now, then press Enter "
                  f"(within {self.timeout - 5} s)...")
            blocked, _ = self.wait_until(lambda s: not s["updateAllowed"], 30)
            code, text = u.finish()
        finally:
            u.close()
        check(code == 409, f"upload when updates were blocked mid-way returned {code}: {text}")
        reason = blocked["blockedReason"]

        code, text = Upload(self.dev, self.corrupt).finish()
        check(code == 409, f"upload while blocked returned {code}: {text}")
        for path in ("/update/reboot", "/update/rollback"):
            code, text = self.dev.post(path)
            check(code == 409, f"POST {path} while blocked returned {code}: {text}")
        if self.partitions_enabled:
            code, text = self.dev.post("/update/partitions/apply")
            check(code == 409, f"partition apply while blocked returned {code}: {text}")
        self.unchanged(before, self.dev.status())

        input("Allow updates again in your application, then press Enter...")
        self.wait_until(lambda s: s["updateAllowed"], 120)
        return f"mid-upload abort and all requests refused with 409, reason '{reason}'"

    # --- restart tests ------------------------------------------------------

    def _install(self, check_restarting=False):
        """Install --firmware into the spare slot and boot it. Returns (before, after, upload s)."""
        before = self.idle()
        check(before["canRollback"] and before["backupPartition"], "device has no second OTA slot")
        t0 = time.time()
        code, text = Upload(self.dev, self.fw).finish()
        check(code == 200, f"upload returned {code}: {text}")
        upload_s = time.time() - t0
        self.restarted = True

        # While the restart is pending (setRestartDelay, 3 s by default), uploads are refused
        if check_restarting:
            try:
                code2, text2 = Upload(self.dev, self.garbage[:8192]).finish(10)
                if code2 == 500 and "restarting" in text2.lower():
                    self.record("upload_while_restarting", "PASS", text2)
                else:
                    self.record("upload_while_restarting", "FAIL", f"returned {code2}: {text2}")
            except (OSError, Fail) as e:
                self.record("upload_while_restarting", "WARN", f"device restarted before it answered ({e})")

        after = self.wait_restart(expect_partition=before["backupPartition"])
        check(after["runningPartition"] == before["backupPartition"],
              f"booted {after['runningPartition']}, expected {before['backupPartition']}")
        check(not after["lastUploadFailed"], "lastUploadFailed set after a good upload")
        check(after["hasBackup"], "no backup reported after an update")
        return before, after, upload_s

    def t_upload_ok(self):
        before, after, upload_s = self._install(check_restarting=True)
        return (f"{len(self.fw)} bytes in {upload_s:.1f} s, now running {after['runningPartition']} "
                f"'{after.get('version', '')}'")

    def t_reboot(self):
        before = self.idle()
        code, text = self.dev.post("/update/reboot")
        check(code == 200, f"reboot returned {code}: {text}")
        self.restarted = True
        after = self.wait_restart(settle=False)
        check(after["runningPartition"] == before["runningPartition"],
              f"came back on {after['runningPartition']}, expected {before['runningPartition']}")

        # Straight after boot the backup slot is still being checked: rollback must wait
        if after.get("backupChecked", True):
            self.record("rollback_while_checking", "SKIP", "backup check had already finished when the device came back")
        else:
            code, text = self.dev.post("/update/rollback")
            if code == 200:
                self.wait_restart()
                self.record("rollback_while_checking", "FAIL", "rollback accepted before the backup check finished")
            elif code == 409:
                self.record("rollback_while_checking", "PASS", text)
            else:
                self.record("rollback_while_checking", "FAIL", f"returned {code}: {text}")
        settled, waited = self.wait_until(lambda s: s.get("backupChecked", True), 60)
        return (f"back on {after['runningPartition']}; backup check finished, hasBackup="
                f"{settled['hasBackup']}")

    def t_rollback(self):
        before = self.idle()
        if before["runningPartition"] == self.original["runningPartition"]:
            raise Skip("still on the original firmware - run upload_ok first")
        check(before["hasBackup"], "no backup reported")
        after = self.rollback_to(before["backupPartition"])
        self.restarted = True
        return f"back on {after['runningPartition']} '{after.get('version', '')}'"

    def t_concurrent(self):
        before = self.idle()
        if before["runningPartition"] != self.original["runningPartition"]:
            self.rollback_to(self.original["runningPartition"])
            before = self.idle()
        check(before["canRollback"], "device has no second OTA slot")
        # First upload is the good image, the second one damaged within the part it sends.
        # The web server closes an upload connection idle for 3 s, so both keep sending:
        # whichever is not the main one trickles 1 KB per 16 KB of the other.
        first = Upload(self.dev, self.fw)
        second = None
        second_open = True

        def trickle(u, cap):
            nonlocal second_open
            if u is second and not second_open:
                return
            try:
                u._send(min(u.sent + 1024, cap))
            except OSError:
                if u is second:
                    second_open = False
                else:
                    raise

        try:
            first.send_to(0.3)
            # Send the whole second upload while the first is still running: it should be
            # refused with 409 and the first one carry on
            second = Upload(self.dev, self.corrupt_early)
            first_cap = int(len(first.body) * 0.6)
            while second.sent < len(second.body):
                second._send(min(second.sent + 16384, len(second.body)))
                trickle(first, first_cap)
            try:
                code2, text2 = second.response(30)
            except (OSError, Fail) as e:
                code2, text2 = 0, str(e)
            code1, text1 = first.finish()
        finally:
            first.close()
            if second:
                second.close()

        if code1 == 200:
            self.restarted = True
            after = self.wait_restart(expect_partition=before["backupPartition"])
            check(after["runningPartition"] == before["backupPartition"], "first upload did not boot")
            check(code2 == 409, f"first upload completed, but the second got {code2} instead of 409: {text2}")
            return f"second upload refused with 409 ({text2}); first completed and booted"
        after = self.idle()
        self.unchanged(before, after)
        raise Fail(f"starting a second upload broke the first (first got {code1}: {text1}; second got "
                   f"{code2}: {text2}). The image check stopped the result from booting.")

    def t_soak(self):
        n = self.args.soak
        if n <= 0:
            raise Skip("run with --soak N")
        home = self.original["runningPartition"]
        heaps = []
        for i in range(n):
            self.rollback_to(home)
            _, after, upload_s = self._install()
            self.rollback_to(home)
            heaps.append(self.dev.status()["freeHeap"])
            self.info(f"round {i + 1}/{n}: upload {upload_s:.1f} s, heap after rollback {heaps[-1]}")
        return f"{n} install + rollback rounds, heap after boot {min(heaps)}..{max(heaps)}"

    def restore(self):
        home = self.original["runningPartition"]
        try:
            s = self.idle()
            if s["runningPartition"] == home:
                self.record("restore", "PASS", f"running the original firmware on {home}")
                return
            s = self.rollback_to(home)
            self.record("restore", "PASS", f"rolled back to the original firmware on {home}")
        except (Fail, OSError, http.client.HTTPException) as e:
            self.record("restore", "FAIL", f"could not return to {home}: {e}. The device runs the "
                        f"--firmware build; OTA your own firmware to it from the page.")

    # --- run ------------------------------------------------------------------

    def setup(self):
        self.wait_online(10)
        s = self.idle()   # also waits for a just-booted device's backup check
        self.original = s
        code, headers, body = self.dev.request("GET", "/update", auth=False)
        self.auth_required = code == 401
        if self.auth_required:
            if not self.dev.creds:
                sys.exit("Device requires credentials - pass --user and --password")
            code, _, _ = self.dev.request("GET", "/update")
            if code != 200:
                sys.exit(f"Credentials refused (GET /update returned {code})")
        # Without the partition editor, /update/partitions falls through to the /update page
        code, headers, _ = self.dev.request("GET", "/update/partitions")
        self.partitions_enabled = code == 200 and "json" in headers.get("content-type", "")
        print(f"Device {self.dev.host}: CustomOTA {s.get('libraryVersion', '?')}, running "
              f"{s.get('runningPartition')}, auth {'on' if self.auth_required else 'off'}, partition editor "
              f"{'on' if self.partitions_enabled else 'off'}, upload timeout {self.timeout} s\n")

    def selected(self):
        only = set(self.args.only.split(",")) if self.args.only else None
        for name, kind, fn, _ in self.tests:
            if only is not None and name not in only:
                continue
            if only is None:
                if kind == "restart" and self.args.no_restart:
                    continue
                if kind == "fs" and self.fs is None:
                    continue
                if kind == "gate" and not self.args.gate:
                    continue
                if name == "soak" and self.args.soak <= 0:
                    continue
            yield name, fn

    def main(self):
        self.setup()
        try:
            for name, fn in self.selected():
                self.run(name, fn)
        except KeyboardInterrupt:
            print("\nInterrupted")
        finally:
            if self.stalled_upload:
                self.stalled_upload[0].close()
            if self.restarted:
                self.restore()

        counts = {}
        for _, result, _ in self.results:
            counts[result] = counts.get(result, 0) + 1
        print("\n" + ", ".join(f"{counts.get(r, 0)} {r}" for r in ("PASS", "FAIL", "WARN", "SKIP")))
        for name, result, detail in self.results:
            if result in ("FAIL", "WARN"):
                print(f"  {result} {name}: {detail}")
        return 1 if counts.get("FAIL") else 0


def main():
    p = argparse.ArgumentParser(description="Test a CustomOTA device over HTTP.",
                                formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    p.add_argument("--host", help="device IP or hostname")
    p.add_argument("--port", type=int, default=80)
    p.add_argument("--firmware", help="a CustomOTA firmware.bin for this device")
    p.add_argument("--user", help="OTA username, if the device has credentials")
    p.add_argument("--password", default="")
    p.add_argument("--upload-timeout", type=int, default=30, help="device's setUploadTimeout() in seconds")
    p.add_argument("--fs", help="SPIFFS image to test filesystem upload with (replaces SPIFFS contents)")
    p.add_argument("--partitions", help="partitions.bin to upload and cancel (never applied)")
    p.add_argument("--gate", action="store_true", help="run the interactive update gate test")
    p.add_argument("--soak", type=int, default=0, help="install + rollback this many times")
    p.add_argument("--no-restart", action="store_true", help="only run tests that don't restart the device")
    p.add_argument("--only", help="comma-separated test names to run (see --list)")
    p.add_argument("--list", action="store_true", help="list the tests and exit")
    args = p.parse_args()

    if args.list:
        for name, kind, desc in TESTS:
            print(f"  {name:22} {kind:8} {desc}")
        return 0
    if not args.host or not args.firmware:
        p.error("--host and --firmware are required")
    return Tester(args).main()


if __name__ == "__main__":
    sys.exit(main())
