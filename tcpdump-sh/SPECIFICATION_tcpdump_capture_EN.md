# tcpdump capture specification

## 1. Purpose

The system captures network traffic through `tcpdump`, rotates dump files by size, compresses completed files, and controls disk usage.

## 2. Capture

Default values:

```text
protocol: udp
port: 5012
interface: any
```

`TCPDUMP_PROTOCOL` and `TCPDUMP_PORT` must be configurable.

Rotation is performed through `tcpdump -C FILE_SIZE_MB`.

The number of final dump files is limited by `MAX_DUMP_FILES`.

## 3. File layout

```text
dumps/       final compressed files only
dumps/tmp/   raw and in-progress files
```

Raw or partially compressed files must not be published in `dumps`.

A final file may appear in `dumps` only after compression has completed successfully.

## 4. Shutdown policy

| Trigger | Mode | Required behavior |
|---|---|---|
| `SIGINT` | graceful | Stop capture, finish active compression, recover remaining raw files |
| `MAX_DUMP_FILES` | graceful | Stop capture and finish allowed files |
| low disk space | graceful-limited | Stop capture, finish active compression, do not start recovery |
| `SIGTERM` | immediate | Stop capture and compression immediately |
| `SIGTERM` during graceful | immediate | Upgrade shutdown to immediate |

## 5. Low disk space

Condition:

```text
free_percent <= MIN_FREE_PERCENT
```

After the threshold is reached:

- no new dump files are created;
- no new recovery compression is started;
- already running compression may finish;
- remaining staging files are removed during cleanup.

## 6. Process model

`tcpdump` is started through `setsid`.

`tcpdump` and its postrotate processes run in a separate process group.

The main script is not part of that process group.

Immediate shutdown must wait for processes that may still write into `dumps/tmp` before removing `tmp`.

## 7. Compression

`bbuadm_gzip.sh`:

1. receives the raw filename from `tcpdump -z`;
2. determines the rotation index;
3. compresses the file into a temporary output under `tmp`;
4. moves the completed compressed file into `dumps`;
5. removes the raw input file.

On `SIGTERM`, the active compressor is stopped and the partial output is removed.

## 8. Required invariants

After a handled shutdown:

- `dumps/tmp` does not exist;
- `dumps` contains no raw pcap files;
- `dumps` contains no `.part` files;
- `dumps` contains only fully completed compressed files.

Loss of unfinished data after `SIGTERM` is acceptable.

Cleanup after `SIGKILL` is not guaranteed.
