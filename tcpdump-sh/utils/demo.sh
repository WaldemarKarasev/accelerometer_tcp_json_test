#!/bin/bash

set -u

SCENARIO="${1:-}"

CAPTURE_PID=""
IPERF_PID=""

echo "Working dir: $(pwd)"
echo "Scenario: $SCENARIO"

# Common demo configuration
export INPUT_DUMP_FILE_MASK=final_file
export CAPTURE_PROTOCOL="${CAPTURE_PROTOCOL:-udp}"
export CAPTURE_PORT="${CAPTURE_PORT:-5012}"

start_capture() {
    ./bbuadm_tcpdump_capture.sh &
    CAPTURE_PID=$!

    echo "Capture PID: $CAPTURE_PID"

    sleep 1
}

start_traffic() {
    ./utils/iperf_mock.sh &
    IPERF_PID=$!

    echo "iperf PID: $IPERF_PID"
}

stop_traffic() {
    if [[ -n "$IPERF_PID" ]] &&
       kill -0 "$IPERF_PID" 2>/dev/null; then

        kill -TERM "$IPERF_PID" 2>/dev/null || true
        wait "$IPERF_PID" 2>/dev/null || true
    fi
}

case "$SCENARIO" in

    sigint)
        export FILE_SIZE_MB=10
        export MAX_DUMP_FILES=100

        export TIME=60
        export RATE=1G

        start_capture
        start_traffic

        sleep 5

        echo "Send SIGINT"
        kill -INT "$CAPTURE_PID"

        wait "$CAPTURE_PID"
        RC=$?

        stop_traffic

        echo "Capture exit code: $RC"
        ;;

    sigterm)
        export FILE_SIZE_MB=10
        export MAX_DUMP_FILES=100

        export TIME=60
        export RATE=1G

        start_capture
        start_traffic

        sleep 5

        echo "Send SIGTERM"
        kill -TERM "$CAPTURE_PID"

        wait "$CAPTURE_PID"
        RC=$?

        stop_traffic

        echo "Capture exit code: $RC"
        ;;

    sigint_sigterm)
        export FILE_SIZE_MB=10
        export MAX_DUMP_FILES=100

        export TIME=60
        export RATE=1G

        start_capture
        start_traffic

        sleep 5

        echo "Send SIGINT"
        kill -INT "$CAPTURE_PID"

        sleep 1

        if kill -0 "$CAPTURE_PID" 2>/dev/null; then
            echo "Send SIGTERM"
            kill -TERM "$CAPTURE_PID"
        fi

        wait "$CAPTURE_PID"
        RC=$?

        stop_traffic

        echo "Capture exit code: $RC"
        ;;

    max_files)
        export FILE_SIZE_MB=10
        export MAX_DUMP_FILES=3

        export TIME=60
        export RATE=1G

        start_capture
        start_traffic

        wait "$CAPTURE_PID"
        RC=$?

        stop_traffic

        echo "Capture exit code: $RC"
        ;;

    disk_threshold)
        if [[ -z "${MIN_FREE_PERCENT:-}" ]]; then
            echo "MIN_FREE_PERCENT must be specified"
            echo
            echo "Check current disk usage:"
            echo "  df -h ./dumps"
            echo
            echo "Example:"
            echo "  MIN_FREE_PERCENT=89 $0 disk_threshold"
            exit 1
        fi

        export MIN_FREE_PERCENT

        # Prevent MAX_DUMP_FILES from stopping the test first
        export FILE_SIZE_MB=100
        export MAX_DUMP_FILES=10000

        # Generate traffic long enough to decrease free disk space
        export TIME="${TIME:-300}"
        export RATE="${RATE:-2G}"

        echo "Disk threshold: ${MIN_FREE_PERCENT}% free"

        start_capture
        start_traffic

        wait "$CAPTURE_PID"
        RC=$?

        stop_traffic

        echo "Capture exit code: $RC"
        ;;

    *)
        echo "Usage:"
        echo "  $0 sigint"
        echo "  $0 sigterm"
        echo "  $0 sigint_sigterm"
        echo "  $0 max_files"
        echo "  MIN_FREE_PERCENT=<percent> $0 disk_threshold"
        exit 1
        ;;
esac