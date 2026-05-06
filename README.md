# dns-server

Product name: `dns-server`.
Repository name: `dns-server`.
GitHub repository: [matoanbach/dns-server](https://github.com/matoanbach/dns-server)

`dns-server` is a systems programming project that implements a small DNS server in C++. It is a hands-on exercise in UDP socket programming, binary protocol parsing, DNS message serialization, question/answer handling, and basic forwarding to an upstream resolver.

## What It Does

This project includes:
- A UDP server that binds to port `2053`.
- DNS header parsing and serialization.
- DNS question parsing for one or more questions in a packet.
- Basic DNS answer parsing and reconstruction.
- Domain-name encoding into DNS label format.
- Parsing support for compressed question names that use DNS pointers.
- Response construction that preserves the request ID and question count.
- Forwarding mode via `--resolver <ip:port>`.
- Per-question forwarding to an upstream DNS server and answer aggregation into the returned response.

## Tech Stack

- C++23
- POSIX UDP sockets
- CMake
- vcpkg manifest files are present, though there are no third-party dependencies declared right now

## Project Layout

- Server entrypoint: `src/server.cpp`
- Core DNS server logic: `src/mydns.cpp`, `src/mydns.h`
- Build configuration: `CMakeLists.txt`
- Local build/run helper: `dns.sh`
- Dependency manifest: `vcpkg.json`, `vcpkg-configuration.json`
- Reference images/docs: `pics/`

## Run Locally

Requirements:
- A C++ compiler with C++23 support.
- CMake `3.13+`.
- `vcpkg` only if you want to use the helper script exactly as written.

Build using the local helper script:

```bash
./dns.sh
```

Build manually with CMake:

```bash
cmake -B build -S .
cmake --build ./build
```

Run the server manually after building:

```bash
./build/server
```

The server listens on:

```text
127.0.0.1:2053
```

Query it with `dig`:

```bash
dig @127.0.0.1 -p 2053 +noedns example.com
```

Run in forwarding mode:

```bash
./build/server --resolver 8.8.8.8:53
```

## Command-Line Usage

Supported runtime flag:
- `--resolver <ip:port>`

Behavior:
- when provided, the server forwards each incoming question to the configured upstream DNS resolver
- the upstream answers are parsed and copied into the final response sent back to the client

## How It Works

### Socket Setup

The server:
- creates a UDP socket
- enables `SO_REUSEPORT`
- binds to `0.0.0.0:2053`
- enters a loop receiving DNS packets with `recvfrom`

This setup lives in `DNS::run()`.

### Request Handling Flow

At a high level, the runtime flow is:

1. Receive a raw UDP packet.
2. Deserialize the DNS header and question section.
3. Convert question names into internal string form.
4. For each question, build a single-question forwarding request.
5. Send that request to the upstream resolver when forwarding mode is enabled.
6. Parse the upstream answer and append it to the outgoing response.
7. Serialize the final DNS response and send it back to the original client.

### DNS Message Model

The code models DNS packets with explicit structs:
- `DNSHeader`
- `DNSQuestion`
- `DNSAnswer`
- `DNSMessage`

This keeps the project readable for people studying the protocol layout.

### Name Encoding

Domain names are encoded as DNS labels using a `{length}{content}` format ending in `0x00`.

For example, `example.com` becomes a sequence like:

```text
07 example 03 com 00
```

The project also includes logic for parsing compressed question names that use pointer bytes with the `0xC0` prefix.

### Response Construction

The response path:
- preserves the original request ID
- marks the packet as a response
- mirrors the question count into the answer count for the constructed reply
- copies upstream answer data into a newly serialized response packet

## Current Behavior And Limitations

The current code is best understood as an educational forwarding DNS server, not a production-ready resolver.

Current behavior notes:
- The server is UDP-only.
- The main happy path is forwarding questions to another resolver.
- The implementation is centered on A-record style answers with IPv4 `rdata` stored as a `uint32_t`.
- Multiple questions in a single request are handled by forwarding them one at a time.
- The code includes parsing for compressed names in the question section.

Current limitations:
- There are no automated tests in the repo right now.
- There is no CI workflow in the repo right now.
- The project does not implement a full recursive resolver with caching.
- The answer parsing path currently reads only a single answer object from the response buffer.
- Header parsing is simplified and does not fully document or validate every flag combination.
- The server is focused on IPv4-style answer payloads, not broad RR-type coverage.
- Debug output is still present in the runtime path.

## Protocol Notes

The code works directly with core DNS packet sections:
- header
- question
- answer

Fields represented in the header model include:
- packet ID
- flags
- question count
- answer count
- authority count
- additional count

This makes the repo useful for learning how DNS packets move between raw bytes and structured data.

## What To Improve

Protocol correctness:
- Expand support for more DNS record types beyond the current simplified answer handling.
- Improve validation of malformed packets and unexpected flag combinations.
- Handle multiple answer records and more complete DNS sections more robustly.

Resolver behavior:
- Add local resolution behavior when no upstream resolver is configured.
- Add caching with TTL handling.
- Add better support for recursive-resolution semantics and failure responses.

Engineering quality:
- Add automated tests for serialization, deserialization, compressed names, and forwarding behavior.
- Add CI for Linux/macOS builds.
- Split protocol parsing and forwarding logic into smaller units for easier maintenance.

Reliability and observability:
- Reduce ad hoc debug output or replace it with structured logs.
- Add clearer error handling around upstream send/receive failures.
- Add timeout handling and retry behavior for upstream resolver communication.
