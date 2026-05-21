import asyncio
import websockets
import json
import time

async def test_insert():
    async with websockets.connect("ws://127.0.0.1:8080/ws") as websocket:
        # Wait for a stats broadcast
        stats = await websocket.recv()
        print("Received stats:", stats)
        
        # Send insert
        await websocket.send(json.dumps({"action": "insert", "email": "testuser@example.com", "pass": "secret"}))
        ack = await websocket.recv()
        print("Received ack:", ack)
        
        # Give DB a second to write
        time.sleep(1)

asyncio.run(test_insert())
