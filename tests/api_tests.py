"""HTTP integration checks against an isolated real C++ server.

The two external-device emulators below are TEST CLIENTS, not claims that
MATLAB/Android hardware has run. Only Python's standard library is needed.
"""
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def main():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
    exe = ROOT / "build" / ("minididi.exe" if os.name == "nt" else "minididi")
    if not exe.exists():
        exe = ROOT / "build" / "Release" / "minididi.exe"
    process = subprocess.Popen([str(exe), "--port", str(port), "--web", str(ROOT / "web")],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    def call(path, data=None, expected=200):
        req = urllib.request.Request(f"http://127.0.0.1:{port}/api/{path}",
                                     data=None if data is None else json.dumps(data).encode(),
                                     headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=40) as r:
                assert r.status == expected, (path, r.status)
                return json.load(r)
        except urllib.error.HTTPError as e:
            assert e.code == expected, (path, e.code, e.read().decode())
            return json.loads(e.read())

    try:
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError(process.stderr.read().decode(errors="replace"))
            try:
                call("health")
                break
            except urllib.error.URLError:
                time.sleep(.1)
        else:
            raise RuntimeError("server did not become healthy")
        initial = call("state")
        assert len(initial["vehicles"]) == 5
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/") as r:
            assert "城市调度实验室" in r.read().decode()
        call("control", {"paused": True})
        tick = call("state")["tick"]
        time.sleep(.4)
        assert call("state")["tick"] == tick, "pause did not stop simulation"
        call("orders", {"pickup": {"x": 2, "y": 2}, "dropoff": {"x": 0, "y": 0}}, 400)
        call("orders", {"pickup": {"x": 0.5, "y": 0}, "dropoff": {"x": 1, "y": 0}}, 400)
        call("control", {"strategy": "invalid"}, 400)
        call("control", {"speed": 0}, 400)
        old_speed=call("state")["speed"]
        call("control", {"speed": 12, "strategy": "invalid"}, 400)
        assert call("state")["speed"] == old_speed, "invalid control partly applied"
        call("reset", {"seed": 7, "strategy": "balanced"})
        call("control", {"speed": 15})
        matlab = call("devices/register", {"kind": "matlab"})
        phone = call("devices/register", {"kind": "phone"})
        call("devices/phone", {"session": "wrong", "seq": 1, "pose": {}}, 401)
        packet = {"session": phone["session"], "seq": 1,
                  "pose": {"east": 1.25, "north": -2.5, "yaw": .4},
                  "sensors": {"accelerometer": [0, 0, 9.81], "gyroscope": [0, 0, 0]}}
        call("devices/phone", packet)
        call("devices/phone", packet, 400)
        assert call("state")["phone"]["pose"]["east"] == 1.25
        wrong_pose = {"session": matlab["session"], "seq": 1, "command_id": 0,
                      "pose": {"x": 999, "y": 999, "yaw": 0}}
        call("devices/matlab", wrong_pose, 400)
        seq = 0
        for _ in range(160):
            s = call("state")
            car = next(v for v in s["vehicles"] if v["kind"] == "matlab")
            position = car["waypoint"] if car["has_command"] else car["position"]
            seq += 1
            reply = call("devices/matlab", {"session": matlab["session"], "seq": seq,
                         "command_id": car["command_id"], "pose": {**position, "yaw": 0}})
            assert reply["ok"]
            if reply["vehicle"]["completed"]:
                break
            time.sleep(.08)
        assert reply["vehicle"]["completed"] > 0, "MATLAB protocol never completed a delivery"
        assert call("state")["metrics"]["collisions"] == 0
        time.sleep(5.2)
        s = call("state")
        assert not s["phone"]["connected"]
        assert not next(v for v in s["vehicles"] if v["kind"] == "matlab")["connected"]
        old_cell = next(v for v in s["vehicles"] if v["kind"] == "matlab")["cell"]
        recovered = call("devices/register", {"kind": "matlab"})
        assert recovered["vehicle"]["cell"] == old_cell
        assert len(call("state")["vehicles"]) == 6
        exported = call("export")
        assert len(exported["orders"]) == exported["metrics"]["orders"]
        call("reset", {"seed": 42})
        call("devices/phone", {**packet, "seq": 2}, 401)
        assert len(call("state")["vehicles"]) == 5
        print("PASS HTTP: autonomous start, static assets, pause, validation, sessions, replay rejection")
        print("PASS HTTP: MATLAB protocol delivery, phone telemetry, lease timeout, reconnect, reset, export")
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        process.stderr.close()


if __name__ == "__main__":
    main()
