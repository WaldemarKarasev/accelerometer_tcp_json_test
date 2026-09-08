# tcpdump capture script

`bbuadm_tcpdump_capture.sh` starts `tcpdump`, rotates pcap files by size, and passes closed files to `bbuadm_gzip.sh` for compression.

## Main idea

```text
dumps/
├── final_file_00.xz
├── final_file_01.xz
└── tmp/
    ├── .tcpdump_<run_id>_
    └── .tcpdump_<run_id>_N.xz.part
```

- `dumps/tmp` contains raw and in-progress files.
- `dumps` contains only fully compressed files.
- After a handled shutdown, `dumps/tmp` is removed.

## Main parameters

| Variable | Default | Meaning |
|---|---:|---|
| `INPUT_DUMP_FILE_MASK` | required | Final file prefix |
| `MAX_DUMP_FILES` | `10` | Maximum number of dump files |
| `FILE_SIZE_MB` | `300` | tcpdump rotation size |
| `INTERFACE` | `any` | Capture interface |
| `MIN_FREE_PERCENT` | `10` | Minimum allowed free disk percentage |
| `DISK_CHECK_INTERVAL` | `1` | Disk check interval, seconds |
| `TCPDUMP_PROTOCOL` | `udp` | `udp` or `tcp` |
| `TCPDUMP_PORT` | `5012` | Capture filter port |

Example:

```bash
INPUT_DUMP_FILE_MASK=final_file \
MAX_DUMP_FILES=10 \
FILE_SIZE_MB=300 \
TCPDUMP_PROTOCOL=udp \
TCPDUMP_PORT=5012 \
./bbuadm_tcpdump_capture.sh
```

## Shutdown behavior

### SIGINT

Graceful shutdown:

1. Stop `tcpdump`.
2. Let already running compression processes finish.
3. Compress remaining allowed raw files through `recover_files`.
4. Remove `dumps/tmp`.
5. Exit code: `130`.

### SIGTERM

Immediate shutdown:

1. Stop `tcpdump`, postrotate, and compression processes.
2. Do not start new recovery/compression work.
3. Wait until processes that may still write into `tmp` have stopped.
4. Remove `dumps/tmp`.
5. Keep already completed `.xz` files.
6. Exit code: `143`.

If `SIGTERM` arrives during graceful shutdown, the shutdown mode is upgraded to immediate.

### MAX_DUMP_FILES

Graceful shutdown:

1. The last allowed dump notifies the main script through `USR1`.
2. Stop `tcpdump`.
3. Finish compression of allowed files.
4. Remove `tmp`.
5. Exit code: `0`.

### Low disk space

When:

```text
free_percent <= MIN_FREE_PERCENT
```

the script:

1. Stops `tcpdump`.
2. Allows already running compression to finish.
3. Does not run `recover_files`, to avoid additional disk usage.
4. Removes remaining raw files together with `dumps/tmp`.
5. Exits with code `15`.

If disk space is already below the threshold before capture starts, the script exits with code `13`.

## Guarantees

After a handled shutdown:

- raw files do not remain in `dumps`;
- partial files do not remain in `dumps`;
- `dumps` contains only completed compressed files;
- `dumps/tmp` is removed;
- immediate shutdown keeps only files that were fully completed;
- low-disk shutdown does not start recovery compression.

`SIGKILL` cannot be trapped, so cleanup after `SIGKILL` is not guaranteed.

## Exit codes

| Code | Meaning |
|---:|---|
| `0` | Success / `MAX_DUMP_FILES` reached |
| `10` | Invalid configuration |
| `11` | Dump directory preparation failed |
| `12` | Internal preparation failed |
| `13` | Low disk space before start |
| `14` | Compression/recovery failed |
| `15` | Low disk space during capture |
| `130` | SIGINT |
| `143` | SIGTERM |
| other | Unexpected `tcpdump` exit code |

## Related scripts

- `bbuadm_gzip.sh` — postrotate/compression handler.
- `tcpdump_setup.sh` — capabilities and AppArmor rules.
- `iperf_mock.sh` — local test traffic generator.
- `demo.sh` — scenario demonstration.
- `run_capture.sh` — minimal run example.

Before the first run:

```bash
./tcpdump_setup.sh setup ./bbuadm_gzip.sh
```

Check configuration:

```bash
./tcpdump_setup.sh status ./bbuadm_gzip.sh
```

## Demo

```bash
./demo.sh sigint
./demo.sh sigterm
./demo.sh sigint_sigterm
./demo.sh max_files
```

For the disk-threshold scenario:

```bash
df -h ./dumps
MIN_FREE_PERCENT=89 ./demo.sh disk_threshold
```

Set the threshold slightly below the current free disk percentage so the test does not need to fill most of the disk.
