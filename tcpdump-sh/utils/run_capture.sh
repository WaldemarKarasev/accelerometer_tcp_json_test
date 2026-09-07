#!/bin/bash

set -u

echo "Working dir: $(pwd)"

export INPUT_DUMP_FILE_MASK=final_file
export MAX_DUMP_FILES=3
./bbuadm_tcpdump_capture.sh
