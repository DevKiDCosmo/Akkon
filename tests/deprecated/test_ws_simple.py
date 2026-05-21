import asyncio
import websockets

async def main():
    uri = "ws://localhost:2409/ws"
    try:
        async with websockets.connect(uri) as websocket:
            print("Successfully connected!")
            while True:
                msg = await websocket.recv()
                print("Received:", msg)
    except Exception as e:
        print("Connection failed:", e)

asyncio.run(main())
