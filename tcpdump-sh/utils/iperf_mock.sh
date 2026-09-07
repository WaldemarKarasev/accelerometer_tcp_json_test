#!/bin/bash

IPERF_BIN="${IPERF_BIN:-iperf3}"
TIME="${TIME:-30}"
RATE="${RATE:-1G}"
TCPDUMP_PROTOCOL="${TCPDUMP_PROTOCOL:-udp}"
TCPDUMP_PORT="${TCPDUMP_PORT:-5012}"

SERVER_PID=""
CLIENT_PID=""

cleanup() {
    [[ -n "$CLIENT_PID" ]] && kill "$CLIENT_PID" 2>/dev/null || true
    [[ -n "$SERVER_PID" ]] && kill "$SERVER_PID" 2>/dev/null || true

    wait "$CLIENT_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

${IPERF_BIN} -s -1 -p "$TCPDUMP_PORT" &
SERVER_PID=$!

sleep 1

echo "TCPDUMP_PROTOCOL: $TCPDUMP_PROTOCOL"
echo "TCPDUMP_PORT: $TCPDUMP_PORT"

if [[ "$TCPDUMP_PROTOCOL" == "udp" ]]; then

    "$IPERF_BIN" \
        -c 127.0.0.1 \
        -p "$TCPDUMP_PORT" \
        -u \
        -t "$TIME" \
        -b "$RATE" &

else

    "$IPERF_BIN" \
        -c 127.0.0.1 \
        -p "$TCPDUMP_PORT" \
        -t "$TIME" \
        -b "$RATE" &
fi

CLIENT_PID=$!

wait "$CLIENT_PID"