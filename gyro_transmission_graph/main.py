import asyncio

import serial
import websockets

SERIAL_PORT = "COM22"
BAUD_RATE = 115200

connected_clients = set()


async def read_serial_and_broadcast():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to Arduino on {SERIAL_PORT}")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8").strip()

                if line.count(",") == 2 and connected_clients:
                    await asyncio.gather(
                        *[client.send(line) for client in connected_clients],
                        return_exceptions=True,
                    )

            await asyncio.sleep(0.001)
    except serial.SerialException:
        print(f"Failed to open port {SERIAL_PORT}")


async def handler(websocket):
    connected_clients.add(websocket)
    try:
        async for message in websocket:
            pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.remove(websocket)


async def main():
    server = await websockets.serve(handler, "localhost", 8765)
    print("WebSocket Server started on ws://localhost:8765")

    await asyncio.gather(server.wait_closed(), read_serial_and_broadcast())


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
