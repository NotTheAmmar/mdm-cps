"""
bridge.py  —  SERIAL VERSION
─────────────────────────────────────────────────────────────────────────────
USB Serial ↔ WebSocket bridge for 2-player table tennis controllers.
Replaces the BLE version entirely.

Architecture:
  [Player1 Nano 33 BLE] ──USB serial──┐
                                       ├── bridge.py ──WebSocket── index.html
  [Player2 Nano 33 BLE] ──USB serial──┘

Serial protocol (115200 baud):
  Arduino → bridge:  "DEVICE:P1\\n" or "DEVICE:P2\\n"  (at startup)
                     "D:pitch,roll\\n"                   (25 Hz IMU data)
  Bridge  → Arduino: 'H'                                (hit feedback)

WebSocket messages (same as BLE version — game HTML unchanged):
  Bridge → Game:  {"type":"imu", "p1":{pitch,roll}, "p2":{pitch,roll}}
  Bridge → Game:  {"type":"status", "p1_connected":bool, "p2_connected":bool}
  Game → Bridge:  {"event":"hit", "player":1|2}

Auto-detection:
  Scans /dev/ttyACM* and /dev/ttyUSB* for Arduinos.
  Each reader thread waits for the "DEVICE:Px" startup line to self-identify.

Manual port override (optional):
  python bridge.py --p1 /dev/ttyACM0 --p2 /dev/ttyACM1

Run:
  python3 bridge/bridge.py

Dependencies:
  pip install pyserial>=3.5 websockets>=12.0
─────────────────────────────────────────────────────────────────────────────
"""

import argparse
import asyncio
import http.server
import json
import logging
import os
import sys
import threading
import time
from typing import Optional

import serial
import serial.tools.list_ports
import websockets

# ── Logging ───────────────────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("bridge")

# ── Serial Settings ───────────────────────────────────────────────────────────
BAUD_RATE           = 115200
DEVICE_ID_TIMEOUT_S = 10.0   # seconds to wait for "DEVICE:Px" after port opens
ID_REQUEST_INTERVAL = 1.5    # seconds between '?' identity request pulses
RECONNECT_DELAY_S   = 2.0    # seconds between reconnect attempts

# ── Network Ports ─────────────────────────────────────────────────────────────
WS_HOST   = "localhost"
WS_PORT   = 8765
HTTP_PORT = 8000

# ── Shared State (all writes happen on the asyncio thread via run_coroutine_threadsafe)
imu_state: dict = {
    "p1": {"pitch": 0.0, "roll": 0.0, "connected": False},
    "p2": {"pitch": 0.0, "roll": 0.0, "connected": False},
}

# Active serial.Serial handles — used by ws_handler to write hit feedback
serial_handles: dict[str, Optional[serial.Serial]] = {"p1": None, "p2": None}

# Active WebSocket connections
ws_clients: set = set()

# asyncio event loop — set in main(), used by serial threads
_loop: Optional[asyncio.AbstractEventLoop] = None


# ══════════════════════════════════════════════════════════════════════════════
# Port Discovery
# ══════════════════════════════════════════════════════════════════════════════

def find_arduino_ports() -> list[str]:
    """
    Return candidate serial port paths that look like connected Arduinos.
    Matches /dev/ttyACM*, /dev/ttyUSB*, or known Arduino USB VIDs.
    """
    ARDUINO_VIDS = {0x2341, 0x2A03, 0x239A}   # Arduino, Arduino SA, Adafruit
    found: list[str] = []
    seen:  set[str]  = set()

    for p in serial.tools.list_ports.comports():
        is_acm_usb = any(x in p.device for x in ['/dev/ttyACM', '/dev/ttyUSB', 'COM'])
        is_arduino = (p.vid in ARDUINO_VIDS) if p.vid else False
        if (is_acm_usb or is_arduino) and p.device not in seen:
            found.append(p.device)
            seen.add(p.device)

    return sorted(found)


# ══════════════════════════════════════════════════════════════════════════════
# HTTP Server  (serves game/index.html on http://localhost:8000)
# ══════════════════════════════════════════════════════════════════════════════

_GAME_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "game")
)


class _QuietHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=_GAME_DIR, **kwargs)

    def log_message(self, fmt, *args):
        pass   # suppress per-request access logs

    def do_GET(self):
        if self.path == "/":
            self.path = "/index.html"
        super().do_GET()


def _start_http_server() -> None:
    server = http.server.HTTPServer(("localhost", HTTP_PORT), _QuietHandler)
    server.serve_forever()


# ══════════════════════════════════════════════════════════════════════════════
# WebSocket Layer
# ══════════════════════════════════════════════════════════════════════════════

async def ws_broadcast(message: str) -> None:
    if not ws_clients:
        return
    results = await asyncio.gather(
        *[c.send(message) for c in list(ws_clients)],
        return_exceptions=True,
    )
    dead = {c for c, r in zip(list(ws_clients), results) if isinstance(r, Exception)}
    ws_clients.difference_update(dead)


async def _broadcast_status() -> None:
    await ws_broadcast(json.dumps({
        "type":         "status",
        "p1_connected": imu_state["p1"]["connected"],
        "p2_connected": imu_state["p2"]["connected"],
    }))


async def ws_handler(websocket) -> None:
    ws_clients.add(websocket)
    log.info("WebSocket client connected  (total: %d)", len(ws_clients))
    try:
        await websocket.send(json.dumps({
            "type":         "status",
            "p1_connected": imu_state["p1"]["connected"],
            "p2_connected": imu_state["p2"]["connected"],
        }))
    except Exception:
        pass

    try:
        async for raw in websocket:
            await _handle_ws_message(raw)
    except Exception:
        pass
    finally:
        ws_clients.discard(websocket)
        log.info("WebSocket client disconnected (total: %d)", len(ws_clients))


async def _handle_ws_message(raw: str) -> None:
    """
    {"event":"hit","player":1|2}  →  write b'H' to that paddle's serial port.
    """
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        return

    if data.get("event") == "hit":
        player = data.get("player")
        ser = serial_handles.get(f"p{player}")
        if ser and ser.is_open:
            try:
                ser.write(b"H")   # fire-and-forget; triggers haptic on the Arduino
            except serial.SerialException:
                pass


# ══════════════════════════════════════════════════════════════════════════════
# Serial IMU Coroutines  (called from serial threads via run_coroutine_threadsafe)
# ══════════════════════════════════════════════════════════════════════════════

async def _on_imu_data(player_key: str, pitch: float, roll: float) -> None:
    imu_state[player_key]["pitch"] = round(pitch, 3)
    imu_state[player_key]["roll"]  = round(roll,  3)
    await ws_broadcast(json.dumps({
        "type": "imu",
        "p1": {"pitch": imu_state["p1"]["pitch"], "roll": imu_state["p1"]["roll"]},
        "p2": {"pitch": imu_state["p2"]["pitch"], "roll": imu_state["p2"]["roll"]},
    }))


async def _on_connected(player_key: str, ser: serial.Serial) -> None:
    serial_handles[player_key] = ser
    imu_state[player_key]["connected"] = True
    log.info("[%s] Connected via serial.", player_key.upper())
    await _broadcast_status()


async def _on_disconnected(player_key: str) -> None:
    serial_handles[player_key] = None
    imu_state[player_key]["connected"] = False
    log.warning("[%s] Serial disconnected.", player_key.upper())
    await _broadcast_status()


# ══════════════════════════════════════════════════════════════════════════════
# Serial Reader Thread  (one per port, runs for the lifetime of the process)
# ══════════════════════════════════════════════════════════════════════════════

def _serial_reader_thread(port: str, forced_player_key: Optional[str] = None) -> None:
    """
    Opens a serial port, optionally skips DEVICE: identification if
    forced_player_key is set (used when --p1/--p2 are passed explicitly),
    then streams IMU data into the asyncio event loop.

    Reconnects automatically on any error.
    """
    player_key: Optional[str] = forced_player_key
    ser: Optional[serial.Serial] = None

    while True:
        try:
            log.info("[SERIAL] Opening %s  (baud=%d)...", port, BAUD_RATE)
            ser = serial.Serial(port, BAUD_RATE, timeout=1.0)

            # ── Phase 1: Wait for device identifier ───────────────────────────
            if player_key is None:
                log.info("[SERIAL] Requesting identity from %s ...", port)
                deadline     = time.time() + DEVICE_ID_TIMEOUT_S
                last_request = 0.0

                while time.time() < deadline:
                    # Pulse '?' periodically so the firmware can respond
                    if time.time() - last_request >= ID_REQUEST_INTERVAL:
                        try:
                            ser.write(b"?")
                        except Exception:
                            pass
                        last_request = time.time()

                    # Non-blocking read with short inner timeout
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="ignore").strip()
                    if line.startswith("DEVICE:"):
                        pid = line.split(":")[1].strip()   # e.g. "P1"
                        player_key = f"p{pid[-1].lower()}"  # → "p1"
                        log.info("[SERIAL] %s → identified as %s", port, player_key.upper())
                        break

                if player_key is None:
                    log.warning("[SERIAL] %s: no DEVICE: response in %.0fs. Retrying...",
                                port, DEVICE_ID_TIMEOUT_S)
                    ser.close()
                    time.sleep(RECONNECT_DELAY_S)
                    continue

            # ── Phase 2: Signal connected ─────────────────────────────────────
            asyncio.run_coroutine_threadsafe(_on_connected(player_key, ser), _loop)

            # ── Phase 3: Stream IMU data ──────────────────────────────────────
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="ignore").strip()

                if line.startswith("DEVICE:"):
                    # Board rebooted mid-session; re-confirm identity
                    pid = line.split(":")[1].strip()
                    player_key = f"p{pid[-1].lower()}"
                    log.info("[SERIAL] %s re-identified as %s after reboot.", port, player_key.upper())
                    asyncio.run_coroutine_threadsafe(_on_connected(player_key, ser), _loop)
                    continue

                if line.startswith("D:"):
                    parts = line[2:].split(",")
                    if len(parts) == 2:
                        try:
                            pitch = float(parts[0])
                            roll  = float(parts[1])
                            asyncio.run_coroutine_threadsafe(
                                _on_imu_data(player_key, pitch, roll), _loop
                            )
                        except ValueError:
                            log.debug("[%s] Bad D: line: %r", player_key.upper(), line)

                elif line.startswith("ERROR:"):
                    log.error("[%s] Firmware error: %s", player_key.upper(), line)

        except serial.SerialException as e:
            log.error("[SERIAL] %s: %s", port, e)
        except OSError as e:
            log.error("[SERIAL] %s: OS error: %s", port, e)
        except Exception as e:
            log.error("[SERIAL] %s: unexpected: %s", port, e)
        finally:
            if ser:
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
            if player_key and _loop:
                asyncio.run_coroutine_threadsafe(_on_disconnected(player_key), _loop)

        log.info("[SERIAL] %s: reconnecting in %.0fs...", port, RECONNECT_DELAY_S)
        time.sleep(RECONNECT_DELAY_S)


# ══════════════════════════════════════════════════════════════════════════════
# Entry Point
# ══════════════════════════════════════════════════════════════════════════════

def parse_args():
    parser = argparse.ArgumentParser(
        description="USB Serial ↔ WebSocket bridge for BLE Table Tennis controllers"
    )
    parser.add_argument("--p1", metavar="PORT",
                        help="Serial port for Player 1 (e.g. /dev/ttyACM0). "
                             "Auto-detected if omitted.")
    parser.add_argument("--p2", metavar="PORT",
                        help="Serial port for Player 2 (e.g. /dev/ttyACM1). "
                             "Auto-detected if omitted.")
    return parser.parse_args()


async def main() -> None:
    global _loop
    _loop = asyncio.get_running_loop()

    args = parse_args()

    log.info("=" * 60)
    log.info("  Table Tennis Bridge (Serial)  —  ws://%s:%d", WS_HOST, WS_PORT)
    log.info("=" * 60)

    # ── HTTP server ───────────────────────────────────────────────────────────
    http_thread = threading.Thread(target=_start_http_server, daemon=True)
    http_thread.start()
    log.info("Game served at  ►  http://localhost:%d", HTTP_PORT)
    log.info("Open that URL in your browser.")
    log.info("-" * 60)

    # ── Resolve serial ports ──────────────────────────────────────────────────
    port_map: dict[str, Optional[str]] = {}   # port → forced player key

    if args.p1 or args.p2:
        # Manual override — player keys are known, skip DEVICE: handshake
        if args.p1:
            port_map[args.p1] = "p1"
            log.info("P1 → %s  (forced, no identification needed)", args.p1)
        if args.p2:
            port_map[args.p2] = "p2"
            log.info("P2 → %s  (forced, no identification needed)", args.p2)
    else:
        # Auto-detect — player keys determined via DEVICE: handshake
        ports = find_arduino_ports()
        if not ports:
            log.error("No Arduino serial ports found!")
            log.error("Plug in both Nano 33 BLE Sense boards and retry.")
            log.error("Or specify ports manually:  --p1 /dev/ttyACM0 --p2 /dev/ttyACM1")
            sys.exit(1)
        log.info("Auto-detected %d port(s): %s", len(ports), ", ".join(ports))
        if len(ports) < 2:
            log.warning("Only 1 port found — Player 2 will show as disconnected.")
        for p in ports:
            port_map[p] = None   # None = auto-identify via DEVICE:

    # ── Start serial reader threads ───────────────────────────────────────────
    for port, forced_key in port_map.items():
        t = threading.Thread(
            target=_serial_reader_thread,
            args=(port, forced_key),
            daemon=True,
            name=f"serial-{port}",
        )
        t.start()

    # ── WebSocket server ──────────────────────────────────────────────────────
    ws_server = await websockets.serve(ws_handler, WS_HOST, WS_PORT)
    log.info("WebSocket server on  ws://%s:%d", WS_HOST, WS_PORT)

    await ws_server.wait_closed()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        log.info("Bridge stopped by user.")
        sys.exit(0)
