#!/bin/bash

set -u

# input parameters without default value
#
# INPUT_DUMP_FILE_MASK - must be passed as correct filename

# mask for creating final dump filename
INPUT_DUMP_FILE_MASK="${INPUT_DUMP_FILE_MASK:-}"
DUMP_FILE_MASK="${INPUT_DUMP_FILE_MASK}_%02d"

# input parameters with default value
MAX_DUMP_FILES="${MAX_DUMP_FILES:-10}"
FILE_SIZE_MB="${FILE_SIZE_MB:-300}"
INTERFACE="${INTERFACE:-any}"
MIN_FREE_PERCENT="${MIN_FREE_PERCENT:-10}"

# output dir values for storing compressed dump files
DUMP_DIR="./dumps"
TEMP_DUMP_DIR="${DUMP_DIR}/tmp"

# compress script
POSTROTATE_SCRIPT="./bbuadm_gzip.sh"

# disk threshold monitor params
DISK_CHECK_INTERVAL="${DISK_CHECK_INTERVAL:-1}"

# compress utils setup
GZIP_BIN="xz"
GZIP_FILE_EXTENSION=".xz"

# bbuadm_tcpdump_capture.sh pid
MAIN_PID=$$

# tcpdump binary variables
TCPDUMP_PID=""
TCPDUMP_REAPED=0
TCPDUMP_RC=0
TCPDUMP_PROTOCOL="${TCPDUMP_PROTOCOL:-udp}"
TCPDUMP_PORT="${TCPDUMP_PORT:-5012}"

# tcpdump and all postrotate running bbuadm_gzip.sh scripts process group pid
PGID=""

DISK_MONITOR_PID=""

# final bbuamd_gzip.sh running script started from bbuadm_tcpdump_capture.sh script
FINAL_GZIP_PID=""

SHUTDOWN_MODE="running"
STOP_REASON=""
FINAL_EXIT_CODE=""

STARTED=0


# Exit codes:
#
# 0   - Success / MAX_DUMP_FILES reached
# 10  - Invalid configuration
# 11  - Dump directory preparation failed
# 12  - Internal preparation failed
# 13  - Not enough disk space before start
# 14  - Compression/recovery failed
# 15  - Low disk space during capture
# 130 - SIGINT
# 143 - SIGTERM

EXIT_OK=0
EXIT_CONFIG_ERROR=10
EXIT_DUMP_DIR_ERROR=11
EXIT_PREPARE_ERROR=12
EXIT_DISK_START_ERROR=13
EXIT_COMPRESSION_ERROR=14
EXIT_LOW_DISK=15

EXIT_SIGINT=130
EXIT_SIGTERM=143


LOG_FILE=capture.log
log() {
    local level="$1"
    shift

    local log_str="$(date '+%Y-%m-%d %H:%M:%S') [$level] $*"

    echo "${log_str}" >&2
    echo "${log_str}" >> $LOG_FILE
}


check_config() {
    [[ -n "${INPUT_DUMP_FILE_MASK:-}" ]] || {
        log ERROR "INPUT_DUMP_FILE_MASK is empty"
        return 1
    }

    [[ "$MAX_DUMP_FILES" =~ ^[1-9][0-9]*$ ]] || {
        log ERROR "MAX_DUMP_FILES must be a positive integer"
        return 1
    }

    [[ "$FILE_SIZE_MB" =~ ^[1-9][0-9]*$ ]] || {
        log ERROR "FILE_SIZE_MB must be a positive integer"
        return 1
    }

    [[ "$MIN_FREE_PERCENT" =~ ^[0-9]+$ ]] || {
        log ERROR "MIN_FREE_PERCENT must be an integer"
        return 1
    }

    (( MIN_FREE_PERCENT > 0 && MIN_FREE_PERCENT < 100 )) || {
        log ERROR "MIN_FREE_PERCENT must be between 1 and 99"
        return 1
    }

    [[ "$DUMP_FILE_MASK" == *"%02d"* ]] || {
        log ERROR "DUMP_FILE_MASK must contain %02d"
        return 1
    }

    [[ "$TCPDUMP_PROTOCOL" == "udp" || "$TCPDUMP_PROTOCOL" == "tcp" ]] || {
    log ERROR "TCPDUMP_PROTOCOL must be udp or tcp"
    return 1
    }

    [[ "$TCPDUMP_PORT" =~ ^[0-9]+$ ]] &&
    (( TCPDUMP_PORT > 0 && TCPDUMP_PORT <= 65535 )) || {
        log ERROR "TCPDUMP_PORT must be between 1 and 65535"
        return 1
    }

    command -v tcpdump >/dev/null || {
        log ERROR "tcpdump not found"
        return 1
    }

    command -v setsid >/dev/null || {
        log ERROR "setsid not found"
        return 1
    }

    command -v "$GZIP_BIN" >/dev/null || {
        log ERROR "Compression utility not found: $GZIP_BIN"
        return 1
    }

    [[ -x "$POSTROTATE_SCRIPT" ]] || {
        log ERROR "POSTROTATE_SCRIPT is not executable: $POSTROTATE_SCRIPT"
        return 1
    }

    return 0
}

prepare_dump_dir() {
    [[ -n "$DUMP_DIR" && "$DUMP_DIR" != "/" ]] || {
        log ERROR "Invalid DUMP_DIR: $DUMP_DIR"
        return 1
    }

    rm -rf -- "$DUMP_DIR"/* || {
        log ERROR "Cannot clean DUMP_DIR: $DUMP_DIR"
        return 1
    }

    mkdir -p "$TEMP_DUMP_DIR" || {
        log ERROR "Cannot create TEMP_DUMP_DIR: $TEMP_DUMP_DIR"
        return 1
    }

    [[ -w "$DUMP_DIR" ]] || {
        log ERROR "DUMP_DIR is not writable: $DUMP_DIR"
        return 1
    }

    return 0
}

cleanup_dump_dir() {
    if [[ -d "$TEMP_DUMP_DIR" ]]; then
        rm -rf -- "$TEMP_DUMP_DIR"
    fi
}

prepare() {
    mkdir -p "$TEMP_DUMP_DIR" || return 1

    TEMP_DUMP_DIR="$(cd "$TEMP_DUMP_DIR" && pwd)" || return 1
    POSTROTATE_SCRIPT="$(readlink -f "$POSTROTATE_SCRIPT")" || return 1

    OUTPUT_MASK="${DUMP_DIR}/${DUMP_FILE_MASK}"

    RUN_ID="${MAIN_PID}_$(date +%s)"
    RAW_MASK="${TEMP_DUMP_DIR}/.tcpdump_${RUN_ID}_"    

    export MAIN_PID
    export RAW_MASK
    export OUTPUT_MASK
    export MAX_DUMP_FILES
    export GZIP_BIN
    export GZIP_FILE_EXTENSION
}


raw_name() {
    local index="$1"

    if (( index == 0 )); then
        echo "$RAW_MASK"
    else
        echo "${RAW_MASK}${index}"
    fi
}


final_name() {
    local index="$1"

    printf "$OUTPUT_MASK" "$index"
}


disk_space_is_low() {
    local used_percent

    used_percent=$(df -P "$TEMP_DUMP_DIR" | awk 'NR == 2 { print $5 }')
    used_percent="${used_percent%\%}"

    log INFO "used percent: $used_percent"

    [[ "$used_percent" =~ ^[0-9]+$ ]] || {
        log ERROR "Cannot determine disk usage"
        return 0
    }

    (( 100 - used_percent <= MIN_FREE_PERCENT ))
}


group_is_running() {
    [[ -n "$PGID" ]] &&
        kill -0 -- "-$PGID" 2>/dev/null
}


stop_tcpdump() {
    local signal="${1:-TERM}"

    if [[ -n "$TCPDUMP_PID" ]] &&
       kill -0 "$TCPDUMP_PID" 2>/dev/null; then

        kill -s "$signal" "$TCPDUMP_PID" 2>/dev/null || true
    fi
}


stop_process_group() {
    if group_is_running; then
        kill -TERM -- "-$PGID" 2>/dev/null || true
    fi

    if [[ -n "$FINAL_GZIP_PID" ]] &&
       kill -0 "$FINAL_GZIP_PID" 2>/dev/null; then

        kill -TERM "$FINAL_GZIP_PID" 2>/dev/null || true
    fi
}


request_graceful() {
    local reason="$1"
    local signal="$2"
    local code="$3"

    [[ "$SHUTDOWN_MODE" == "immediate" ]] && return

    if [[ "$SHUTDOWN_MODE" == "running" ]]; then
        SHUTDOWN_MODE="graceful"
        STOP_REASON="$reason"
        FINAL_EXIT_CODE="$code"
    fi

    stop_tcpdump "$signal"
}


request_immediate() {
    local reason="$1"
    local code="$2"

    SHUTDOWN_MODE="immediate"
    STOP_REASON="$reason"
    FINAL_EXIT_CODE="$code"

    stop_process_group
}


sigint_handle() {
    log INFO "SIGINT received"

    request_graceful "sigint" INT "$EXIT_SIGINT"
}


sigterm_handle() {
    log INFO "SIGTERM received"

    request_immediate "sigterm" "$EXIT_SIGTERM"
}


max_files_handle() {
    log INFO "MAX_DUMP_FILES reached"

    request_graceful "max_files" TERM "$EXIT_OK"
}


low_disk_handle() {
    log ERROR "Free disk space <= ${MIN_FREE_PERCENT}%"

    request_graceful "low_disk" TERM "$EXIT_LOW_DISK"
    # request_immediate "low_disk" "$EXIT_LOW_DISK"
}


disk_monitor() {
    while kill -0 "$MAIN_PID" 2>/dev/null; do

        if disk_space_is_low; then
            kill -USR2 "$MAIN_PID" 2>/dev/null
            return
        fi

        sleep "$DISK_CHECK_INTERVAL"
    done
}


wait_tcpdump() {
    local rc

    (( TCPDUMP_REAPED )) && return

    while true; do

        wait "$TCPDUMP_PID"
        rc=$?

        if kill -0 "$TCPDUMP_PID" 2>/dev/null; then
            continue
        fi

        TCPDUMP_RC="$rc"
        TCPDUMP_REAPED=1

        return
    done
}


wait_postrotate_processes() {
    while group_is_running; do

        [[ "$SHUTDOWN_MODE" == "immediate" ]] && return 1

        sleep 0.2
    done

    return 0
}


run_gzip_handler() {
    local rc

    "$POSTROTATE_SCRIPT" "$@" &
    FINAL_GZIP_PID=$!

    while true; do

        wait "$FINAL_GZIP_PID"
        rc=$?

        if kill -0 "$FINAL_GZIP_PID" 2>/dev/null; then

            if [[ "$SHUTDOWN_MODE" == "immediate" ]]; then
                kill -TERM "$FINAL_GZIP_PID" 2>/dev/null || true
            fi

            continue
        fi

        FINAL_GZIP_PID=""

        return "$rc"
    done
}

recover_files() {
    local i
    local raw
    local final

    for (( i=0; i<MAX_DUMP_FILES; i++ )); do

        [[ "$SHUTDOWN_MODE" == "immediate" ]] && return

        raw="$(raw_name "$i")"
        final="$(final_name "$i")${GZIP_FILE_EXTENSION}"

        [[ -f "$raw" ]] || continue

        if [[ -f "$final" ]]; then
            log INFO "Already compressed: $final"
            continue
        fi

        log INFO "Compress remaining raw file: $raw"

        if ! run_gzip_handler "$raw"; then

            [[ "$SHUTDOWN_MODE" == "immediate" ]] && return

            log ERROR "Failed to compress: $raw"
            FINAL_EXIT_CODE="$EXIT_COMPRESSION_ERROR"
        fi
    done
}


graceful_finish() {
    log INFO "Graceful shutdown: $STOP_REASON"

    stop_tcpdump TERM
    wait_tcpdump

    wait_postrotate_processes || {
        return
    }

    [[ "$SHUTDOWN_MODE" == "immediate" ]] && return

    # Prevent additional disk usage during low disk shutdown
    if [[ "$STOP_REASON" == "low_disk" ]]; then
        return
    fi

    recover_files
}


immediate_finish() {
    log INFO "Immediate shutdown: $STOP_REASON"

    stop_process_group

    wait_tcpdump

    while group_is_running; do
        sleep 0.1
    done

    if [[ -n "$FINAL_GZIP_PID" ]]; then
        wait "$FINAL_GZIP_PID" 2>/dev/null || true
        FINAL_GZIP_PID=""
    fi
}


stop_disk_monitor() {
    if [[ -n "$DISK_MONITOR_PID" ]] &&
       kill -0 "$DISK_MONITOR_PID" 2>/dev/null; then

        kill -TERM "$DISK_MONITOR_PID" 2>/dev/null || true
        wait "$DISK_MONITOR_PID" 2>/dev/null || true
    fi
}


exit_handle() {
    local rc=$?

    trap - EXIT

    [[ -z "$FINAL_EXIT_CODE" ]] &&
        FINAL_EXIT_CODE="$rc"

    if (( STARTED )); then

        if [[ "$SHUTDOWN_MODE" == "running" ]]; then
            SHUTDOWN_MODE="graceful"
            STOP_REASON="script_exit"
        fi

        if [[ "$SHUTDOWN_MODE" == "immediate" ]]; then
            immediate_finish
        else
            graceful_finish

            if [[ "$SHUTDOWN_MODE" == "immediate" ]]; then
                immediate_finish
            fi
        fi
    fi

    stop_disk_monitor

    cleanup_dump_dir

    trap - INT TERM USR1 USR2

    exit "$FINAL_EXIT_CODE"
}

trap sigint_handle INT
trap sigterm_handle TERM
trap max_files_handle USR1
trap low_disk_handle USR2
trap exit_handle EXIT


check_config || {
    log ERROR "Invalid configuration"
    exit "$EXIT_CONFIG_ERROR"
}

prepare_dump_dir || {
    log ERROR "Failed to prepare dump directory"
    exit "$EXIT_DUMP_DIR_ERROR"
}

prepare || {
    log ERROR "Preparation failed"
    exit "$EXIT_PREPARE_ERROR"
}

if disk_space_is_low; then
    log ERROR "Not enough free disk space"
    exit "$EXIT_DISK_START_ERROR"
fi

disk_monitor &
DISK_MONITOR_PID=$!


log INFO "Starting tcpdump"
log INFO "Max files: $MAX_DUMP_FILES"
log INFO "File size: ${FILE_SIZE_MB} MB"
log INFO "Output mask: $OUTPUT_MASK"

log INFO "MAIN_PID: $MAIN_PID"
log INFO "RAW_MASK: $RAW_MASK"
log INFO "GZIP_BIN: $GZIP_BIN"
log INFO "GZIP_FILE_EXTENSION: $GZIP_FILE_EXTENSION"

setsid tcpdump \
    -i "$INTERFACE" \
    -nn \
    -s 0 \
    -U \
    -C "$FILE_SIZE_MB" \
    -w "$RAW_MASK" \
    -z "$POSTROTATE_SCRIPT" \
    "$TCPDUMP_PROTOCOL" port "$TCPDUMP_PORT" &

TCPDUMP_PID=$!
PGID=$TCPDUMP_PID

STARTED=1

log INFO "tcpdump PID=$TCPDUMP_PID PGID=$PGID"


wait_tcpdump


if [[ "$SHUTDOWN_MODE" == "running" ]]; then
    SHUTDOWN_MODE="graceful"
    STOP_REASON="tcpdump_exit"
    FINAL_EXIT_CODE="$TCPDUMP_RC"
fi

[[ -z "$FINAL_EXIT_CODE" ]] &&
    FINAL_EXIT_CODE="$TCPDUMP_RC"

exit "$FINAL_EXIT_CODE"