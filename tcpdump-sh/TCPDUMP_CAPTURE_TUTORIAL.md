# tcpdump capture setup and demo tutorial

This document describes how to prepare the environment, run the capture script, and execute the demo scenarios.

## 1. Required files

Expected project files:

```text
bbuadm_tcpdump_capture.sh
bbuadm_gzip.sh
tcpdump_setup.sh
run_capture.sh
demo.sh
utils/iperf_mock.sh
```

Required system tools:

```text
tcpdump
xz
iperf3
setcap
AppArmor tools
```

## 2. Prepare the environment

Make the scripts executable:

```bash
chmod +x bbuadm_tcpdump_capture.sh
chmod +x bbuadm_gzip.sh
chmod +x tcpdump_setup.sh
chmod +x demo.sh
chmod +x utils/iperf_mock.sh
```

Configure tcpdump capabilities and AppArmor rules:

```bash
./tcpdump_setup.sh setup ./bbuadm_gzip.sh
```

Check the configuration:

```bash
./tcpdump_setup.sh status ./bbuadm_gzip.sh
```

The setup script configures tcpdump so the main capture script can run without `sudo`.

## 3. Run the capture script

`INPUT_DUMP_FILE_MASK` is required.

Minimal example:

```bash
INPUT_DUMP_FILE_MASK=final_file ./bbuadm_tcpdump_capture.sh
```

With custom settings:

```bash
INPUT_DUMP_FILE_MASK=final_file MAX_DUMP_FILES=5 FILE_SIZE_MB=100 TCPDUMP_PROTOCOL=udp TCPDUMP_PORT=5012 ./bbuadm_tcpdump_capture.sh
```

The output files will look like:

```text
dumps/final_file_00.xz
dumps/final_file_01.xz
...
```

A minimal wrapper example is available in:

```text
run_capture.sh
```

## 4. Run the demo

The demo starts the capture script and generates local traffic through `iperf3`.

The capture script and `iperf_mock.sh` must use the same:

```text
TCPDUMP_PROTOCOL
TCPDUMP_PORT
```

Default values:

```text
udp
5012
```

Available scenarios:

```bash
./demo.sh sigint
./demo.sh sigterm
./demo.sh sigint_sigterm
./demo.sh max_files
```

### SIGINT

Demonstrates graceful shutdown and recovery of remaining raw files.

```bash
./demo.sh sigint
```

### SIGTERM

Demonstrates immediate shutdown.

```bash
./demo.sh sigterm
```

### SIGINT followed by SIGTERM

Demonstrates upgrade from graceful shutdown to immediate shutdown.

```bash
./demo.sh sigint_sigterm
```

### MAX_DUMP_FILES

Demonstrates automatic stop after the configured number of dump files.

```bash
./demo.sh max_files
```

## 5. Demo low disk behavior

This scenario uses the real filesystem and real `df` output.

First check current disk space:

```bash
df -h ./dumps
```

Example: if approximately `90%` is free, choose a threshold slightly below it:

```bash
MIN_FREE_PERCENT=89 ./demo.sh disk_threshold
```

The demo generates traffic until captured data reduces free disk space enough to reach the threshold.

Do not set the threshold far below the current free-space value. The purpose of the demo is to trigger the condition after a small amount of generated data, not to fill most of the disk.

## 6. Custom protocol or port

Example with another UDP port:

```bash
TCPDUMP_PORT=6000 ./demo.sh sigint
```

Example with TCP:

```bash
TCPDUMP_PROTOCOL=tcp TCPDUMP_PORT=5012 ./demo.sh sigint
```

Both the capture and traffic generator must receive the same values.

## 7. Cleanup environment configuration

If the tcpdump-specific capabilities and AppArmor rules are no longer needed:

```bash
./tcpdump_setup.sh cleanup ./bbuadm_gzip.sh
```
