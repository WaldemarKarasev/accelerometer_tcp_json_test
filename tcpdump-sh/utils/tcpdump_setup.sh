#!/bin/bash

set -e

ACTION="${1:-}"
POSTROTATE_SCRIPT="${2:-./bbuadm_gzip.sh}"

TCPDUMP="$(command -v tcpdump)"
POSTROTATE_SCRIPT="$(readlink -f "$POSTROTATE_SCRIPT")"

APPARMOR_PROFILE="/etc/apparmor.d/usr.bin.tcpdump"
APPARMOR_LOCAL="/etc/apparmor.d/local/usr.bin.tcpdump"

RULES=(
    "$POSTROTATE_SCRIPT ixr,"
    "/usr/bin/date ixr,"
    "/usr/bin/bash ixr,"
    "/usr/bin/dash ixr,"
    "/usr/bin/mv ixr,"
    "/usr/bin/gzip ixr,"
    "/usr/bin/xz ixr,"
    "/usr/bin/rm ixr,"
    "signal,"
)

add_rule() {
    local rule="$1"

    grep -qxF "$rule" "$APPARMOR_LOCAL" 2>/dev/null ||
        echo "$rule" | sudo tee -a "$APPARMOR_LOCAL" >/dev/null
}

remove_rule() {
    local rule="$1"

    sudo sed -i "\|^${rule}$|d" "$APPARMOR_LOCAL"
}

reload_apparmor() {
    sudo apparmor_parser -r "$APPARMOR_PROFILE"
}

setup() {
    echo "Configuring tcpdump"

    sudo setcap cap_net_raw,cap_net_admin=eip "$TCPDUMP"

    chmod +x "$POSTROTATE_SCRIPT"

    sudo mkdir -p "$(dirname "$APPARMOR_LOCAL")"
    sudo touch "$APPARMOR_LOCAL"

    for rule in "${RULES[@]}"; do
        add_rule "$rule"
    done

    reload_apparmor

    echo "Setup complete"
}

cleanup() {
    echo "Cleaning tcpdump configuration"

    sudo setcap -r "$TCPDUMP" 2>/dev/null || true

    if [[ -f "$APPARMOR_LOCAL" ]]; then
        for rule in "${RULES[@]}"; do
            remove_rule "$rule"
        done
    fi

    reload_apparmor

    echo "Cleanup complete"
}

status() {
    echo "Capabilities:"
    getcap "$TCPDUMP"

    echo
    echo "AppArmor rules:"

    for rule in "${RULES[@]}"; do
        if grep -qxF "$rule" "$APPARMOR_LOCAL" 2>/dev/null; then
            echo "[OK] $rule"
        else
            echo "[MISSING] $rule"
        fi
    done
}

restart() {
    cleanup
    setup
}

case "$ACTION" in
    setup)
        setup
        ;;
    cleanup)
        cleanup
        ;;
    restart)
        restart
        ;;
    status)
        status
        ;;
    *)
        echo "Usage:"
        echo "  $0 setup [postrotate_script]"
        echo "  $0 cleanup [postrotate_script]"
        echo "  $0 restart [postrotate_script]"
        echo "  $0 status [postrotate_script]"
        exit 1
        ;;
esac