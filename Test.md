#!/bin/bash

set -u

GZIP_PID=""


log() {
    local level="$1"
    shift

    echo "[$level] gzip.sh[$$]: $*" >&2
}


sigint_handle() {
    # Ничего не останавливаем.
    # gzip должен закончить текущий файл.
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

    exit 143
}


compress_file() {
    local file="$1"
    local rc

    log INFO "Compress: $file"

    gzip "$file" &
    GZIP_PID=$!

    while true; do

        wait "$GZIP_PID"
        rc=$?

        # wait был прерван SIGINT,
        # но gzip продолжает работать.
        if kill -0 "$GZIP_PID" 2>/dev/null; then
            continue
        fi

        GZIP_PID=""

        return "$rc"
    done
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


    # Файл уже сверх лимита.
    # Не переименовываем и не сжимаем.
    if (( INDEX >= MAX_DUMP_FILES )); then
        log INFO "Overflow dump: $file"
        kill -USR1 "$MAIN_PID" 2>/dev/null || true
        return 0
    fi


    printf -v new_file "$OUTPUT_MASK" "$INDEX"


    # Старые файлы не перезаписываем.
    if [[ -e "$new_file" || -e "$new_file.gz" ]]; then
        log ERROR "Destination already exists: $new_file"

        kill -TERM "$MAIN_PID" 2>/dev/null || true
        return 1
    fi


    mv -- "$file" "$new_file" || {
        log ERROR "Cannot rename $file"

        kill -TERM "$MAIN_PID" 2>/dev/null || true
        return 1
    }


    # Последний разрешённый файл уже закрыт tcpdump
    # и получил окончательное имя.
    #
    # Сообщаем основному скрипту, что пора
    # останавливать tcpdump.
    if (( INDEX == MAX_DUMP_FILES - 1 )); then
        log INFO "Last dump reached: index=$INDEX"

        kill -USR1 "$MAIN_PID" 2>/dev/null || true
    fi


    compress_file "$new_file"
}


trap sigint_handle INT
trap sigterm_handle TERM


# Используется capture.sh при recovery:
#
# gzip.sh --ready test_dump_03
#
# Файл уже имеет правильное имя,
# его надо только досжать.
if [[ "${1:-}" == "--ready" ]]; then

    [[ -n "${2:-}" ]] || exit 1

    compress_file "$2"
    exit $?
fi


[[ -n "${1:-}" ]] || {
    log ERROR "Dump file is not specified"
    exit 1
}


postrotate "$1"
exit $?




capture.sh
#!/bin/bash

set -u

MAX_DUMP_FILES="${MAX_DUMP_FILES:-10}"
FILE_SIZE_MB="${FILE_SIZE_MB:-300}"
DUMP_DIR="${DUMP_DIR:-./dumps}"
INTERFACE="${INTERFACE:-any}"

# Итоговая маска:
# test_dump_00
# test_dump_01
# ...
DUMP_FILE_MASK="${DUMP_FILE_MASK:-test_dump_%02d}"

POSTROTATE_SCRIPT="${POSTROTATE_SCRIPT:-./gzip.sh}"

MIN_FREE_PERCENT="${MIN_FREE_PERCENT:-10}"
DISK_CHECK_INTERVAL="${DISK_CHECK_INTERVAL:-1}"


MAIN_PID=$$

TCPDUMP_PID=""
TCPDUMP_REAPED=0
TCPDUMP_RC=0

PGID=""

DISK_MONITOR_PID=""
FINAL_GZIP_PID=""

SHUTDOWN_MODE="running"
STOP_REASON=""
FINAL_EXIT_CODE=""

STARTED=0


log() {
    local level="$1"
    shift

    echo "$(date '+%Y-%m-%d %H:%M:%S') [$level] $*" >&2
}


check_config() {
    [[ "$MAX_DUMP_FILES" =~ ^[1-9][0-9]*$ ]] || return 1
    [[ "$FILE_SIZE_MB" =~ ^[1-9][0-9]*$ ]] || return 1
    [[ "$MIN_FREE_PERCENT" =~ ^[0-9]+$ ]] || return 1

    (( MIN_FREE_PERCENT > 0 && MIN_FREE_PERCENT < 100 )) || return 1

    [[ "$DUMP_FILE_MASK" == *"%02d"* ]] || return 1

    command -v tcpdump >/dev/null || return 1
    command -v setsid >/dev/null || return 1

    [[ -x "$POSTROTATE_SCRIPT" ]] || return 1

    return 0
}


prepare() {
    mkdir -p "$DUMP_DIR" || return 1

    DUMP_DIR="$(cd "$DUMP_DIR" && pwd)" || return 1
    POSTROTATE_SCRIPT="$(readlink -f "$POSTROTATE_SCRIPT")" || return 1

    OUTPUT_MASK="${DUMP_DIR}/${DUMP_FILE_MASK}"

    # Уникальное внутреннее имя только для этого запуска.
    RUN_ID="${MAIN_PID}_$(date +%s)"
    RAW_MASK="${DUMP_DIR}/.tcpdump_${RUN_ID}"

    export MAIN_PID
    export RAW_MASK
    export OUTPUT_MASK
    export MAX_DUMP_FILES
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


check_existing_files() {
    local i
    local file

    for (( i=0; i<MAX_DUMP_FILES; i++ )); do
        file="$(final_name "$i")"

        if [[ -e "$file" || -e "$file.gz" ]]; then
            log ERROR "File already exists: $file"
            return 1
        fi
    done

    return 0
}


disk_space_is_low() {
    local total
    local available

    read -r total available < <(
        df -Pk "$DUMP_DIR" |
        awk 'NR == 2 { print $2, $4 }'
    )

    [[ "$total" =~ ^[0-9]+$ ]] || return 0
    [[ "$available" =~ ^[0-9]+$ ]] || return 0

    (( available * 100 <= total * MIN_FREE_PERCENT ))
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

    request_graceful "sigint" INT 130
}


sigterm_handle() {
    log INFO "SIGTERM received"

    request_immediate "sigterm" 143
}


max_files_handle() {
    log INFO "MAX_DUMP_FILES reached"

    request_graceful "max_files" TERM 0
}


low_disk_handle() {
    log ERROR "Free disk space <= ${MIN_FREE_PERCENT}%"

    request_immediate "low_disk" 2
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

        # wait мог прерваться нашим trap.
        # Если tcpdump ещё существует — продолжаем ждать.
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

        # wait был прерван сигналом,
        # но gzip.sh ещё работает.
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
        final="$(final_name "$i")"

        # tcpdump закрыл файл, но -z для него не вызвался.
        if [[ -f "$raw" ]]; then

            log INFO "Compress remaining raw file: $raw"

            run_gzip_handler "$raw" || {
                log ERROR "Failed to compress: $raw"
                FINAL_EXIT_CODE=1
            }

            continue
        fi

        # Файл уже был переименован gzip.sh,
        # но само сжатие не завершилось.
        if [[ -f "$final" && ! -f "$final.gz" ]]; then

            log INFO "Finish compression: $final"

            run_gzip_handler --ready "$final" || {
                log ERROR "Failed to compress: $final"
                FINAL_EXIT_CODE=1
            }
        fi
    done
}


remove_overflow_files() {
    local index="$MAX_DUMP_FILES"
    local file

    while true; do
        file="$(raw_name "$index")"

        [[ -e "$file" ]] || break

        log INFO "Remove overflow file: $file"
        rm -f -- "$file"

        index=$((index + 1))
    done
}


graceful_finish() {
    log INFO "Graceful shutdown: $STOP_REASON"

    stop_tcpdump TERM
    wait_tcpdump

    # Ждём gzip.sh, которые tcpdump уже запустил через -z.
    wait_postrotate_processes || {
        return
    }

    [[ "$SHUTDOWN_MODE" == "immediate" ]] && return

    if [[ "$STOP_REASON" == "max_files" ]]; then
        remove_overflow_files
    fi

    # Проверяем недосжатые разрешённые файлы.
    recover_files
}


immediate_finish() {
    log INFO "Immediate shutdown: $STOP_REASON"

    stop_process_group
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

    # Чтобы exit внутри этой функции не вызвал её повторно.
    trap - EXIT

    [[ -z "$FINAL_EXIT_CODE" ]] &&
        FINAL_EXIT_CODE="$rc"

    if (( STARTED )); then

        # Скрипт завершился сам, не через известный сценарий.
        if [[ "$SHUTDOWN_MODE" == "running" ]]; then
            SHUTDOWN_MODE="graceful"
            STOP_REASON="script_exit"
        fi

        if [[ "$SHUTDOWN_MODE" == "immediate" ]]; then
            immediate_finish
        else
            graceful_finish

            # Во время graceful мог прийти SIGTERM
            # или закончиться место.
            if [[ "$SHUTDOWN_MODE" == "immediate" ]]; then
                immediate_finish
            fi
        fi
    fi

    stop_disk_monitor

    trap - INT TERM USR1 USR2

    exit "$FINAL_EXIT_CODE"
}


trap sigint_handle INT
trap sigterm_handle TERM

# Внутренний сигнал от gzip.sh:
# последний разрешённый файл начал обработку.
trap max_files_handle USR1

# Внутренний сигнал от disk monitor.
trap low_disk_handle USR2

trap exit_handle EXIT


check_config || {
    log ERROR "Invalid configuration"
    exit 1
}


prepare || {
    log ERROR "Preparation failed"
    exit 1
}


check_existing_files || exit 1


if disk_space_is_low; then
    log ERROR "Not enough free disk space"
    exit 2
fi


disk_monitor &
DISK_MONITOR_PID=$!


log INFO "Starting tcpdump"
log INFO "Max files: $MAX_DUMP_FILES"
log INFO "File size: ${FILE_SIZE_MB} MB"
log INFO "Output mask: $OUTPUT_MASK"


# setsid создаёт отдельную process group/session для:
#
# tcpdump
#   ├── gzip.sh
#   │    └── gzip
#   └── ...
#
# Сам capture.sh в эту группу не входит.
setsid tcpdump \
    -i "$INTERFACE" \
    -nn \
    -s 0 \
    -U \
    -C "$FILE_SIZE_MB" \
    -w "$RAW_MASK" \
    -z "$POSTROTATE_SCRIPT" \
    -- "$@" &

TCPDUMP_PID=$!
PGID=$TCPDUMP_PID

STARTED=1

log INFO "tcpdump PID=$TCPDUMP_PID PGID=$PGID"


wait_tcpdump


# Если tcpdump завершился самостоятельно.
if [[ "$SHUTDOWN_MODE" == "running" ]]; then
    SHUTDOWN_MODE="graceful"
    STOP_REASON="tcpdump_exit"
    FINAL_EXIT_CODE="$TCPDUMP_RC"
fi

[[ -z "$FINAL_EXIT_CODE" ]] &&
    FINAL_EXIT_CODE="$TCPDUMP_RC"

exit "$FINAL_EXIT_CODE"
