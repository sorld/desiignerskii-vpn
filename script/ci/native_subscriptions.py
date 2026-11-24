"""Exercise real subscription storage and refresh against two independent local feeds."""
import argparse
import base64
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def feed(name, path, bind):
    profiles = [name]
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != path:
                self.send_error(404)
                return
            links = [f"socks5://192.0.2.10:{1080 + n}#{label}" for n, label in enumerate(profiles)]
            data = base64.b64encode("\n".join(links).encode())
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer((bind, 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, f"http://{bind}:{server.server_port}{path}", profiles


def run_test(client, ctl, data, upgrade=None):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        addr = f"127.0.0.1:{sock.getsockname()[1]}"
    def start():
        return subprocess.Popen([str(client), "--headless", "--data-dir", str(data),
                                 "--api-listen", addr, "--no-resume", "--restart-mode", "exit"],
                                cwd=client.parent, stdout=log, stderr=log)
    def call(*args):
        result = subprocess.run([str(ctl), "--addr", addr, "--key-file", str(data / "config/api.key"),
                                 "--timeout", "10s", "--json", *map(str, args)], capture_output=True, stdin=subprocess.DEVNULL, timeout=17)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        return json.loads(result.stdout)
    def ready(proc):
        for _ in range(120):
            if proc.poll() is not None:
                raise RuntimeError(f"Client exited with {proc.returncode}")
            try:
                call("status")
                return
            except RuntimeError:
                time.sleep(.15)
        raise RuntimeError("Throne API did not start")
    def stop(proc):
        if proc and proc.poll() is None:
            try:
                call("shutdown", "--yes")
            except (RuntimeError, ValueError):
                pass
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                raise RuntimeError("Client did not shut down cleanly")
    def groups():
        return {g["name"]: g["id"] for g in call("groups")["groups"]}
    def profiles(gid):
        return {(p["id"], p["name"]) for p in call("profiles", "--group", gid)["profiles"]}

    first, url_a, names_a = feed("One", "/provider/one?format=b64&fixture=one", "127.0.0.1")
    second, url_b, _ = feed("Two", "/some/path?fixture=two&format=list", "localhost")
    proc = None
    try:
        with (data / "test.log").open("wb") as log:
            try:
                proc = start()
                ready(proc)
                for name, url in (("A", url_a), ("B", url_b)):
                    call("sub", "add", url, "--name", name)
                ids = groups()
                a, b = ids["A"], ids["B"]
                assert a != b
                before_a, before_b = profiles(a), profiles(b)
                assert {n for _, n in before_a} == {"One"}, before_a
                assert {n for _, n in before_b} == {"Two"}, before_b
                stop(proc)
                if upgrade:
                    subprocess.run([str(upgrade), "/CURRENTUSER", "/VERYSILENT", "/NORESTART", "/SUPPRESSMSGBOXES"], check=True, timeout=180)
                proc = start()
                ready(proc)
                ids = groups()
                assert (ids["A"], ids["B"]) == (a, b)
                assert (profiles(a), profiles(b)) == (before_a, before_b)
                names_a.append("Three")
                call("sub", "update", a)
                assert {n for _, n in profiles(a)} == {"One", "Three"}
                assert profiles(b) == before_b
                print("PASS: Two providers, persistent groups and profile IDs, independent refresh.")
            finally:
                stop(proc)
    finally:
        for server in (first, second):
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    parser.add_argument("ctl", type=Path)
    parser.add_argument("--install-over", type=Path)
    parser.add_argument("--data-dir", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as folder:
        data = args.data_dir or Path(folder)
        try:
            run_test(args.client.resolve(), args.ctl.resolve(), data, args.install_over)
        except Exception:
            log = data / "test.log"
            if log.is_file():
                print(log.read_text(errors="replace")[-2400:])
            raise
