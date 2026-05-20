# Akkon Server - Run Instructions

## 1. Running the C++ Server

### Prerequisites
- CMake (3.10+)
- A C++17 compatible compiler (GCC, Clang, or MSVC)

### Build & Run
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./Akkon
```
Then navigate to `http://localhost:8080/` in your browser.

## 2. Running the Python Webview Server
If you want to run the webview independently and simulate backend WebSocket metrics, you can use the provided Python server.

### Setup (Virtual Environment)
```bash
cd webview
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

### Run
```bash
python3 server.py
```
This will start the HTTP server on port 8000 and the WebSocket mock on port 8080.
Navigate to `http://localhost:8000/`.

## 3. (Optional) Fancy Vite.js Setup
If you want a modern, fast development server for the frontend, you can use Vite.
```bash
cd webview
npm create vite@latest . -- --template vanilla
npm install
npm run dev
```
Navigate to `http://localhost:5173/` and it will automatically connect to the C++ (or Python) WebSocket server on port 8080!
