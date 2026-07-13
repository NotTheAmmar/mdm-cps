import json

import serial
from flask import Flask, render_template
from flask_socketio import SocketIO

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

SERIAL_PORT = "COM24"
BAUD_RATE = 115200


def read_serial():
    print(f" Checking connection to Arduino on {SERIAL_PORT}...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f" Successfully connected to {SERIAL_PORT}! Listening for data...")

        while True:
            if ser.in_waiting > 0:
                raw_line = ser.readline().decode("utf-8").strip()
                try:
                    data = json.loads(raw_line)
                    # print(f"Python received -> Pitch: {data['pitch']}, Roll: {data['roll']}")
                    socketio.emit("orientation", data)
                except json.JSONDecodeError:
                    pass
            socketio.sleep(0.005)
    except Exception as e:
        print(f"\n❌ SERIAL ERROR: {e}")
        print("1. Did you close the Arduino Serial Monitor?")
        print(f"2. Is your Arduino definitely on {SERIAL_PORT}?\n")


@app.route("/")
def index():
    return render_template("index.html")


@socketio.on("connect")
def handle_connect():
    print(" Browser connected to web server!")


if __name__ == "__main__":
    socketio.start_background_task(read_serial)

    print("Starting web server at http://localhost:5000")
    socketio.run(
        app, host="0.0.0.0", port=5000, debug=False, allow_unsafe_werkzeug=True
    )
