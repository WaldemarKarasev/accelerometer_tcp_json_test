# tcpdump capture specification and behavior

## 1. Purpose

`bbuadm_tcpdump_capture.sh` captures network traffic with `tcpdump`, rotates dump files by size, compresses completed files, limits the number of output files, and monitors free disk space.

The compression handler is `bbuadm_gzip.sh`.

## 2. Input parameters

| Variable | Default | Description |
|---|---:|---|
| `INPUT_DUMP_FILE_MASK` | required | Base name for final dump files |
| `MAX_DUMP_FILES` | `10` | Maximum number of final dump files |
| `FILE_SIZE_MB` | `300` | tcpdump rotation size in MB |
| `INTERFACE` | `any` | Network interface |
| `MIN_FREE_PERCENT` | `10` | Minimum allowed free disk space, percent |
| `DISK_CHECK_INTERVAL` | `1` | Disk check interval in seconds |
| `TCPDUMP_PROTOCOL` | `udp` | Capture protocol: `udp` or `tcp` |
| `TCPDUMP_PORT` | `5012` | Capture port |

`INPUT_DUMP_FILE_MASK` is a plain string without the numeric suffix.

Example:

```bash
INPUT_DUMP_FILE_MASK=final_file
```

The script builds the final mask internally as:

```text
final_file_%02d
```

and produces files such as:

```text
final_file_00.xz
final_file_01.xz
final_file_02.xz
```

## 3. File layout

```text
dumps/
├── final_file_00.xz
├── final_file_01.xz
└── tmp/
    ├── raw dump files
    └── partial compressed files
```

Rules:

- `dumps/tmp` is staging storage.
- Raw and partially compressed files stay only in `dumps/tmp`.
- `dumps` contains only fully completed compressed files.
- `dumps/tmp` is removed after a handled shutdown.

## 4. Capture behavior

`tcpdump` is started in a separate process group through `setsid`.

The capture filter is:

```text
<TCPDUMP_PROTOCOL> port <TCPDUMP_PORT>
```

Default filter:

```text
udp port 5012
```

Rotation is controlled by `FILE_SIZE_MB`.

When a rotated file is closed, `tcpdump -z` starts `bbuadm_gzip.sh`.

The compression handler writes to a temporary `.part` file first. Only after successful compression is the final `.xz` file moved into `dumps`.

## 5. Shutdown behavior

### SIGINT

Graceful shutdown:

1. Stop `tcpdump`.
2. Finish already running compression.
3. Compress remaining allowed raw files.
4. Remove `dumps/tmp`.
5. Exit with code `130`.

### SIGTERM

Immediate shutdown:

1. Stop `tcpdump` and compression processes.
2. Do not start new recovery work.
3. Wait until processes that may write into `dumps/tmp` have stopped.
4. Remove `dumps/tmp`.
5. Keep only already completed `.xz` files.
6. Exit with code `143`.

If `SIGTERM` arrives during graceful shutdown, the mode is upgraded to immediate.

### MAX_DUMP_FILES

When the last allowed dump file is reached:

1. Stop `tcpdump`.
2. Finish compression of allowed files.
3. Remove `dumps/tmp`.
4. Exit with code `0`.

No ring-buffer overwrite is used.

### Low disk space

The script periodically checks:

```text
free_percent <= MIN_FREE_PERCENT
```

When the threshold is reached:

1. Stop `tcpdump`.
2. Allow already running compression to finish.
3. Do not start `recover_files`.
4. Remove remaining staging files during cleanup.
5. Exit with code `15`.

This avoids additional disk usage while preserving files that were already fully completed.

If disk space is already below the threshold before capture starts, the script exits with code `13`.

## 6. Guarantees

After a handled shutdown:

- `dumps` contains only fully completed compressed files;
- raw files are not published in `dumps`;
- `.part` files are not published in `dumps`;
- `dumps/tmp` is removed;
- `SIGTERM` may discard unfinished data;
- low-disk shutdown does not start additional recovery compression.

Cleanup after `SIGKILL` is not guaranteed because `SIGKILL` cannot be trapped.

## 7. Exit codes

| Code | Meaning |
|---:|---|
| `0` | Success / `MAX_DUMP_FILES` reached |
| `10` | Invalid configuration |
| `11` | Dump directory preparation failed |
| `12` | Internal preparation failed |
| `13` | Low disk space before capture start |
| `14` | Compression/recovery failed |
| `15` | Low disk space during capture |
| `130` | SIGINT |
| `143` | SIGTERM |
| other | Unexpected `tcpdump` exit code |
