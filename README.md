# Accelerometer Distributed System

Test task implementation: distributed accelerometer data processing system in C++.

This repository contains a Level 1 implementation based on TCP sockets and JSON messages.

## Architecture

The system consists of three independent processes:

```text
Node A  ->  Server  ->  Node B
Node A  <-  Server  <-  Node B
```

### Node A

Node A is the accelerometer data source.

Responsibilities:

- connects to the server as role `A`;
- generates accelerometer packets with frequency 50 Hz;
- sends accelerometer packets to the server;
- receives calculated acceleration modules from the server;
- writes received modules to `accel/module.log`.

### Server

The server is the central router and validator.

Responsibilities:

- accepts TCP connections from Node A and Node B;
- registers client roles using a `hello` message;
- stores active connections for Node A and Node B;
- receives accelerometer packets from Node A;
- filters consecutive duplicate accelerometer packets;
- forwards valid accelerometer packets to Node B;
- receives acceleration modules from Node B;
- forwards acceleration modules back to Node A.

### Node B

Node B is the data processor.

Responsibilities:

- connects to the server as role `B`;
- receives accelerometer packets from the server;
- computes the acceleration vector module;
- sends the calculated module back to the server.

The acceleration module is calculated as:

```
module = std::sqrt(x^2 + y^2 + z^2)
```

## Implemented Level

Implemented:

- Level 1: TCP sockets + JSON;
- Linux-only deployment option;
- accelerometer data emulation in Node A;
- role registration for Node A and Node B;
- duplicate filtering on the server;
- full data path from Node A to Node B and back;
- writing final results to `accel/module.log`.

Not implemented:

- Level 2 gRPC / Protocol Buffers;
- Level 3 TLS / API key authentication;
- Android NDK sensor integration.

## Build

Requirements:

* CMake 3.16 or newer;
* C++17 compiler;
* Boost.Asio (Boost 1.74);
* nlohmann/json.

Build commands:

```bash
cmake -S . -B build
cmake --build build
```

## Run

Run the programs in three separate terminals.

After startup, Node A begins to generate accelerometer packets and sends them to the server.

## Demonstration scenario

Recommended demo sequence:

```bash
# terminal 1
./build/server
```

```bash
# terminal 2
./build/node_b
```

```bash
# terminal 3
./build/node_a
```

The final result file is created here:

```text
accel/module.log
```
## Linux-only implementation and accelerometer emulation

This implementation uses the Linux-only deployment option allowed by the task.

Node A does not read data from a real Android accelerometer. Instead, it uses a software accelerometer emulator.

The emulator generates synthetic accelerometer values using sinusoidal functions and gravity-like base acceleration on the Y axis. This makes the generated data deterministic enough for debugging and realistic enough for demonstration.

Data generation frequency is approximately 50 Hz.

Since:

```text
1000 ms / 50 = 20 ms
```

Node A generates and sends one packet approximately every 20 milliseconds.