#!/bin/bash

set -u

# Input params from bbuadm_tcpdump_capture.sh
# 
# MAIN_PID - bbuadm_tcpdump_capture.sh pid
# RAW_MASK - dump file mask written by tcpdump
# OUTPUT_MASK - dump file mask with format needed by documentation
# MAX_DUMP_FILES - max dump files number
# GZIP_BIN - binary to compress dump file. default: xz
# GZIP_FILE_EXTENSION - compressed file extension. default: ".xz"

GZIP_PID=""
TMP_COMPRESSED_FILE=""

GZIP_BIN="${GZIP_BIN:-xz}"
GZIP_FILE_EXTENSION="${GZIP_FILE_EXTENSION:-.xz}"
COMPRESS_ARGS=""


LOG_FILE=capture.log
log() {
    local level="$1"
    shift

    local log_str="$(date '+%Y-%m-%d %H:%M:%S') [$level] $*"

    echo "${log_str}" >&2
    echo "${log_str}" >> $LOG_FILE
}


sigint_handle() {
    log INFO "SIGINT received, finish current compression"
}


sigterm_handle() {
    log INFO "SIGTERM received, stop compression"

    trap - TERM

    if [[ -n "$GZIP_PID" ]] &&
       kill -0 "$GZIP_PID" 2>/dev/null; then

        kill -TERM "$GZIP_PID" 2>/dev/null || true
        wait "$GZIP_PID" 2>/dev/null || true
    fi

    if [[ -n "$TMP_COMPRESSED_FILE" ]]; then
        rm -f -- "$TMP_COMPRESSED_FILE"
    fi

    exit 143
}


compress_file() {
    local input_file="$1"
    local output_file="$2"
    local rc

    TMP_COMPRESSED_FILE="${input_file}${GZIP_FILE_EXTENSION}.part"

    rm -f -- "$TMP_COMPRESSED_FILE"

    log INFO "Compress: $input_file"


    $GZIP_BIN $COMPRESS_ARGS \
            < "$input_file" \
            > "$TMP_COMPRESSED_FILE" &
    
    GZIP_PID=$!

    while true; do
        wait "$GZIP_PID"
        rc=$?

        if kill -0 "$GZIP_PID" 2>/dev/null; then
            continue
        fi

        GZIP_PID=""
        break
    done

    if (( rc != 0 )); then
        rm -f -- "$TMP_COMPRESSED_FILE"
        TMP_COMPRESSED_FILE=""
        return "$rc"
    fi

    mv -- "$TMP_COMPRESSED_FILE" "$output_file" || {
        return 1
    }

    TMP_COMPRESSED_FILE=""

    rm -f -- "$input_file" || {
        log ERROR "Cannot remove raw file: $input_file"
        return 1
    }

    return 0
}


get_index() {
    local file="$1"
    local suffix

    if [[ "$file" == "$RAW_MASK" ]]; then
        INDEX=0
        return 0
    fi

    if [[ "$file" != "$RAW_MASK"* ]]; then
        return 1
    fi

    suffix="${file#"$RAW_MASK"}"

    [[ "$suffix" =~ ^[0-9]+$ ]] || return 1

    INDEX=$((10#$suffix))

    return 0
}

postrotate() {
    local file="$1"
    local new_file

    get_index "$file" || {
        log ERROR "Cannot get dump index: $file"
        kill -TERM "$MAIN_PID" 2>/dev/null || true
        return 1
    }


    if (( INDEX >= MAX_DUMP_FILES )); then
        log INFO "Overflow dump: $file"
        kill -USR1 "$MAIN_PID" 2>/dev/null || true
        return 0
    fi


    printf -v new_file "$OUTPUT_MASK" "$INDEX"

    local output_file="${new_file}${GZIP_FILE_EXTENSION}"

    if [[ -e "$output_file" ]]; then
        log ERROR "Destination already exists: $output_file"

        kill -TERM "$MAIN_PID" 2>/dev/null || true
        return 1
    fi

    if (( INDEX == MAX_DUMP_FILES - 1 )); then
        log INFO "Last dump reached: index=$INDEX"

        kill -USR1 "$MAIN_PID" 2>/dev/null || true
    fi

    compress_file "$file" "$output_file"
}


trap sigint_handle INT
trap sigterm_handle TERM


[[ -n "${1:-}" ]] || {
    log ERROR "Dump file is not specified"
    exit 1
}

postrotate "$1"
exit $?