import json
import re
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PORT = 8080


class OrderbookBridge:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.proc = self._start_process()

    def _start_process(self) -> subprocess.Popen:
        candidates = [ROOT / "orderbook_build.exe", ROOT / "orderbook"]
        exe = next((c for c in candidates if c.exists()), None)
        if exe is None:
            raise FileNotFoundError("Build missing. Compile orderbook.cpp first.")
        proc = subprocess.Popen(
            [str(exe), "--api"],
            cwd=str(ROOT),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        return proc

    def _send(self, command: str) -> str:
        if self.proc.poll() is not None:
            self.proc = self._start_process()
        assert self.proc.stdin is not None
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()
        return self._read_until_end_marker(self.proc)

    @staticmethod
    def _read_until_end_marker(proc: subprocess.Popen) -> str:
        assert proc.stdout is not None
        out_lines = []
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            if line.strip() == "__END__":
                break
            out_lines.append(line)
        return "".join(out_lines)

    def _get_book(self) -> dict:
        raw = self._send("book")
        bids = []
        asks = []
        side = None
        for line in raw.splitlines():
            t = line.strip()
            if t == "Bids:":
                side = "bids"
                continue
            if t == "Asks:":
                side = "asks"
                continue
            m = re.search(r"px=(\d+)\s+qty=(\d+)", t)
            if not m:
                continue
            level = {"price": int(m.group(1)), "quantity": int(m.group(2))}
            if side == "bids":
                bids.append(level)
            elif side == "asks":
                asks.append(level)
        return {"bids": bids, "asks": asks}

    def execute(self, command: str) -> dict:
        with self.lock:
            output = self._send(command)
            trades = []
            for line in output.splitlines():
                m = re.search(
                    r"Trade \| bidId=(\d+)\s+askId=(\d+)\s+qty=(\d+)\s+bidPx=(\d+)\s+askPx=(\d+)",
                    line.strip(),
                )
                if m:
                    trades.append(
                        {
                            "bidId": int(m.group(1)),
                            "askId": int(m.group(2)),
                            "quantity": int(m.group(3)),
                            "bidPrice": int(m.group(4)),
                            "askPrice": int(m.group(5)),
                        }
                    )
            book = self._get_book()
            return {"ok": True, "command": command, "trades": trades, "book": book}


BRIDGE = OrderbookBridge()


class Handler(BaseHTTPRequestHandler):
    def _send_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        data = self.rfile.read(length) if length else b"{}"
        return json.loads(data.decode("utf-8"))

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self) -> None:
        if self.path in ("/", "/index.html"):
            html = (ROOT / "index.html").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)
            return
        if self.path == "/styles.css":
            css = (ROOT / "styles.css").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/css; charset=utf-8")
            self.send_header("Content-Length", str(len(css)))
            self.end_headers()
            self.wfile.write(css)
            return
        if self.path == "/app.js":
            js = (ROOT / "app.js").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/javascript; charset=utf-8")
            self.send_header("Content-Length", str(len(js)))
            self.end_headers()
            self.wfile.write(js)
            return
        if self.path == "/api/book":
            payload = BRIDGE.execute("book")
            self._send_json(200, payload)
            return
        self._send_json(404, {"ok": False, "error": "Not found"})

    def do_POST(self) -> None:
        try:
            data = self._read_json()
            if self.path == "/api/add":
                cmd = "add {id} {side} {qty} {type} {price}".format(
                    id=int(data["id"]),
                    side=str(data["side"]).lower(),
                    qty=int(data["qty"]),
                    type=str(data["type"]).lower(),
                    price=int(data.get("price", 0)),
                )
                self._send_json(200, BRIDGE.execute(cmd))
                return
            if self.path == "/api/cancel":
                cmd = f"cancel {int(data['id'])}"
                self._send_json(200, BRIDGE.execute(cmd))
                return
            if self.path == "/api/modify":
                cmd = "modify {id} {side} {qty} {price}".format(
                    id=int(data["id"]),
                    side=str(data["side"]).lower(),
                    qty=int(data["qty"]),
                    price=int(data["price"]),
                )
                self._send_json(200, BRIDGE.execute(cmd))
                return
            self._send_json(404, {"ok": False, "error": "Unknown endpoint"})
        except Exception as exc:
            self._send_json(400, {"ok": False, "error": str(exc)})


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print(f"Server started at http://127.0.0.1:{PORT}")
    server.serve_forever()
