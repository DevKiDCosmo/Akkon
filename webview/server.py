import asyncio
import websockets
import json
import threading
import http.server
import socketserver
import os
import random

def start_http_server():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    Handler = http.server.SimpleHTTPRequestHandler
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", 3509), Handler) as httpd:
        print("HTTP server running at http://localhost:3509/")
        httpd.serve_forever()

async def ws_handler(websocket):
    allocated = 1048576
    capacity = 10485760
    total = 0
    while True:
        try:
            total += random.randint(1, 10)
            hit_rate = 70.0 + (random.random() * 20.0)
            data = {
                "allocated": allocated,
                "capacity": capacity,
                "total_queries": total,
                "hit_rate": hit_rate
            }
            await websocket.send(json.dumps(data))
            await asyncio.sleep(1)
        except websockets.exceptions.ConnectionClosed:
            break
        except Exception:
            break

async def start_ws_server():
    print("WebSocket server running at ws://localhost:8080/ws")
    async with websockets.serve(ws_handler, "localhost", 8080):
        await asyncio.Future()

if __name__ == "__main__":
    t = threading.Thread(target=start_http_server, daemon=True)
    t.start()
    asyncio.run(start_ws_server())
