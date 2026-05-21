# Akkon Interface Overview

This document outlines all current and planned APIs, communication protocols, and network interfaces exposed by the Akkon server.

## 1. Internal Interfaces (Memory & Architecture)

- **Memory Domains**:
    - `RUNTIME`: Base execution structures.
    - `REQUEST`: Transient memory tied to client requests.
    - `RESERVED`: System-critical allocation space.
    - `VERIFICATION (UNITTEST)`: Isolated validation operations.
    - `LIFECYCLE`: Lockdown and administrative overrides.

- **Storage Engine (`db::DataStorage`)**:
    - `information` table: Stores `uuid`, `identifier` (pre-hashed), and `pwk` (pre-hashed) mappings across multiple physical `.db` shards using round-robin insertion.
    - Structure: `uuid TEXT` (derived from hashing concatenated identifier and pwk), `identifier TEXT` (64-character SHA-256 hash), `pwk TEXT` (64-character SHA-256 hash).

## 2. HTTP REST APIs

All modern client interactions are routed via the manual, lightweight HTTP REST parser.

- **`POST /api/v1/insert`**
    - **Purpose**: Registers a new user into the database shards and bloom filter engine.
    - **Payload**: JSON format `{"identifier": "<64-character hex SHA-256 hash>", "pwk": "<64-character hex SHA-256 hash>"}`.
    - **Pre-requisite**: Values must be pre-hashed on the client-side before transmission.
    - **Server-side Validation**: Strictly validates that both fields are exactly 64 characters in length.
    - **Response**:
        - `200 OK` on success: `{"status": "success"}`
        - `400 Bad Request` if field lengths violate constraints or fields are missing: `{"error": "invalid fields or size limits (must be 64 char hashes)"}`

- **`GET /api/v1/exists?identifier=...`**
    - **Purpose**: Queries the in-memory bloom filter and exact store to check if a user identifier exists.
    - **Query Parameter**: `identifier` (MUST be a 64-character hex SHA-256 hash).
    - **Server-side Validation**: Strictly validates that `identifier` query parameter is exactly 64 characters long.
    - **Response**:
        - `200 OK` on success: `{"result": "DEFINITELY_YES" | "PROBABLY_YES" | "DEFINITELY_NO"}`
        - `400 Bad Request` if field length violates constraints or is missing: `{"error": "invalid identifier size (must be 64 char hash)"}`

## 3. WebSocket Streams

- **`ws://127.0.0.1:2409/ws`**
    - **Purpose**: Real-time dashboard synchronization. The server broadcasts a status payload every 2 seconds. The socket does NOT accept inbound JSON execution commands.
    - **Handshake Connection Protocol**: Complies fully with RFC 6455. The server extracts the `Sec-WebSocket-Key` header, appends the GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`, calculates the 20-byte SHA-1 hash, base64 encodes it, and returns the result in the `Sec-WebSocket-Accept` header.
    - **Broadcast Payload**: `{"action":"stats", "reliability": <score 0-100>, "disk_space": <bytes>, "ram_allocated": <bytes>, "ram_capacity": <bytes>}`

## 4. Akkon Native Protocol

- **`AKKON ...`**
    - **Purpose**: Reserved native communication protocol for inner-cluster communication.
    - **Current Response**: `AKKON_ACK\n`
    - **Planned**: Direct binary data stream integration between Akkon microservices.

More: ftp, ssh etc.