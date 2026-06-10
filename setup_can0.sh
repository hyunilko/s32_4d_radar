#!/usr/bin/env bash
# Setup SocketCAN interface can0 using the PEAK PCAN-USB FD adapter.
#
# Usage:
#   sudo ./setup_can0.sh           — bring up can0 now (first-time / recovery)
#   sudo ./setup_can0.sh --install — install systemd service for auto-start on boot
#   sudo ./setup_can0.sh --remove  — remove the systemd service

set -euo pipefail

# ── Install / remove helpers ──────────────────────────────────────────────────
SYSTEMD_SERVICE="can0-setup.service"
SYSTEMD_UNIT="/etc/systemd/system/${SYSTEMD_SERVICE}"
CONFIGURE_SCRIPT="/usr/local/sbin/can0-configure.sh"
MODULES_LOAD_CONF="/etc/modules-load.d/peak_usb.conf"

do_install() {
    [[ $EUID -ne 0 ]] && exec sudo bash "$0" --install

    echo "Installing can0 auto-start..."

    # 1. Auto-load peak_usb on boot
    echo "peak_usb" > "${MODULES_LOAD_CONF}"

    # 2. Minimal configure script — waits for interface; CAN FD params are
    #    applied by the ROS node (SocketCanFdTransport::can_link_up).
    cat > "${CONFIGURE_SCRIPT}" <<'EOF'
#!/usr/bin/env bash
IFACE=can0
WAIT_SEC=20

for i in $(seq 1 $((WAIT_SEC * 2))); do
    ip link show "${IFACE}" &>/dev/null && break
    sleep 0.5
    [[ $i -eq $((WAIT_SEC * 2)) ]] && echo "ERROR: ${IFACE} not found after ${WAIT_SEC}s" && exit 1
done
echo "${IFACE} is present — CAN FD params will be applied by the ROS node."
EOF
    chmod +x "${CONFIGURE_SCRIPT}"

    # 3. systemd unit
    cat > "${SYSTEMD_UNIT}" <<EOF
[Unit]
Description=Configure CAN0 interface (PEAK PCAN-USB FD)
After=systemd-modules-load.service
Wants=systemd-modules-load.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=${CONFIGURE_SCRIPT}
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable "${SYSTEMD_SERVICE}"
    echo "Installed and enabled '${SYSTEMD_SERVICE}'."
    echo "can0 will come up automatically on every boot."
}

do_remove() {
    [[ $EUID -ne 0 ]] && exec sudo bash "$0" --remove
    systemctl disable "${SYSTEMD_SERVICE}" 2>/dev/null || true
    rm -f "${SYSTEMD_UNIT}" "${CONFIGURE_SCRIPT}" "${MODULES_LOAD_CONF}"
    systemctl daemon-reload
    echo "Removed can0 auto-start."
}

case "${1:-}" in
    --install) do_install; exit 0 ;;
    --remove)  do_remove;  exit 1 ;;
esac

IFACE="${1:-can0}"
WAIT_SEC=10
PEAK_VENDOR_ID="0c72"
PCAN_UDEV_RULE="/etc/udev/rules.d/45-pcan.rules"
PCAN_UDEV_BACKUP="/etc/udev/rules.d/45-pcan.rules.disabled"
PCAN_BLACKLIST="/etc/modprobe.d/blacklist-pcan.conf"

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GRN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YLW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

if [[ $EUID -ne 0 ]]; then
    error "Root privileges required. Re-running with sudo..."
    exec sudo bash "$0" "$@"
fi

# ── 1. Find PEAK USB device and interface in sysfs ────────────────────────────
info "Looking for PEAK USB device (vendor=${PEAK_VENDOR_ID})..."
USB_DEV_ID=""
for d in /sys/bus/usb/devices/*/; do
    vid_file="${d}idVendor"
    if [[ -f "${vid_file}" ]] && [[ "$(cat "${vid_file}")" == "${PEAK_VENDOR_ID}" ]]; then
        USB_DEV_ID=$(basename "${d}")
        info "Found PEAK device: ${USB_DEV_ID} (product=$(cat ${d}idProduct 2>/dev/null || echo '?'))"
        break
    fi
done

if [[ -z "${USB_DEV_ID}" ]]; then
    error "No PEAK USB device found (vendor ${PEAK_VENDOR_ID}). Is the adapter plugged in?"
    exit 1
fi

# The USB interface node (e.g. 1-10:1.0) is what peak_usb binds to.
# Find it dynamically — the suffix may differ on partial enumeration.
USB_IFACE_ID=""
for d in /sys/bus/usb/devices/${USB_DEV_ID}:*/; do
    [[ -d "$d" ]] && USB_IFACE_ID=$(basename "$d") && break
done
if [[ -z "${USB_IFACE_ID}" ]]; then
    warn "No USB interfaces found for '${USB_DEV_ID}' — device may need re-enumeration."
fi

# ── 2. Block pcan from auto-loading via module alias (0c72:0012 → pcan) ───────
# udev rules alone are not enough: the kernel loads pcan via modules.alias on
# every USB re-enumeration event.  A modprobe.d blacklist + install override
# prevents any code path from loading pcan.
if [[ ! -f "${PCAN_BLACKLIST}" ]]; then
    info "Blacklisting 'pcan' module to prevent auto-load on USB events..."
    printf 'blacklist pcan\ninstall pcan /bin/false\n' > "${PCAN_BLACKLIST}"
fi

# Disable the pcan udev rule as well
if [[ -f "${PCAN_UDEV_RULE}" ]]; then
    info "Disabling pcan udev rule (${PCAN_UDEV_RULE})..."
    mv "${PCAN_UDEV_RULE}" "${PCAN_UDEV_BACKUP}"
fi
udevadm control --reload-rules

# ── 3. Unload proprietary pcan driver ─────────────────────────────────────────
if lsmod | grep -q '^pcan'; then
    info "Unloading proprietary 'pcan' module..."
    modprobe -r pcan 2>/dev/null || rmmod --force pcan 2>/dev/null || true
    sleep 1
    if lsmod | grep -q '^pcan'; then
        warn "Could not fully unload 'pcan' — proceeding anyway"
    fi
fi

# ── 4. Load kernel peak_usb SocketCAN driver ──────────────────────────────────
if ! lsmod | grep -q 'peak_usb'; then
    info "Loading 'peak_usb' kernel module..."
    if ! modprobe peak_usb; then
        error "modprobe peak_usb failed. Try: sudo apt install linux-modules-extra-\$(uname -r)"
        [[ -f "${PCAN_UDEV_BACKUP}" ]] && mv "${PCAN_UDEV_BACKUP}" "${PCAN_UDEV_RULE}"
        exit 1
    fi
fi

# ── 5. Re-enumerate USB device via usb driver unbind/bind ─────────────────────
# authorized toggle fails with EPROTO when pcan left the device in a stale state.
# Unbinding from the 'usb' bus driver removes all interfaces, then rebind triggers
# full re-enumeration — peak_usb (the only loaded CAN driver) claims it automatically.
# Perform a USB hardware reset via USBDEVFS_RESET ioctl.
# This is equivalent to a physical replug: the device re-enumerates from scratch.
# With pcan blacklisted, peak_usb is now the only driver that will probe it.
BUSNUM=$(cat "/sys/bus/usb/devices/${USB_DEV_ID}/busnum" 2>/dev/null)
DEVNUM=$(cat "/sys/bus/usb/devices/${USB_DEV_ID}/devnum" 2>/dev/null)
if [[ -z "${BUSNUM}" || -z "${DEVNUM}" ]]; then
    error "Could not read bus/device number for '${USB_DEV_ID}'."
    error "Please physically replug the PCAN-USB FD adapter, then re-run this script."
    exit 1
fi
DEV_PATH=$(printf "/dev/bus/usb/%03d/%03d" "${BUSNUM}" "${DEVNUM}")
info "Performing USB hardware reset on ${DEV_PATH}..."
_USB_DEV_PATH="${DEV_PATH}" python3 - <<'PYEOF'
import fcntl, sys, os
USBDEVFS_RESET = 0x5514
dev = os.environ["_USB_DEV_PATH"]
try:
    with open(dev, "wb") as f:
        fcntl.ioctl(f, USBDEVFS_RESET, 0)
    print("  USB reset OK")
except Exception as e:
    print(f"  USB reset failed: {e}", file=sys.stderr)
    sys.exit(1)
PYEOF
sleep 2

# ── 6. Wait for the interface to appear ───────────────────────────────────────
info "Waiting up to ${WAIT_SEC}s for '${IFACE}' to appear..."
for i in $(seq 1 $((WAIT_SEC * 2))); do
    if ip link show "${IFACE}" &>/dev/null; then
        info "Interface '${IFACE}' found."
        break
    fi
    sleep 0.5
    if [[ $i -eq $((WAIT_SEC * 2)) ]]; then
        error "Interface '${IFACE}' did not appear after ${WAIT_SEC}s."
        CAN_IFS=$(ls /sys/class/net/ | grep '^can' || true)
        if [[ -n "${CAN_IFS}" ]]; then
            warn "Found CAN interfaces: ${CAN_IFS}"
            warn "Re-run with: sudo ./setup_can0.sh ${CAN_IFS%% *}"
        else
            error "No CAN interfaces found at all. Check: dmesg | grep -i peak"
        fi
        [[ -f "${PCAN_UDEV_BACKUP}" ]] && mv "${PCAN_UDEV_BACKUP}" "${PCAN_UDEV_RULE}" && udevadm control --reload-rules
        exit 1
    fi
done

# ── 7. Configure CAN FD parameters ───────────────────────────────────────────
# CAN FD parameters are configured by the ROS node (SocketCanFdTransport::can_link_up).
# The node uses 'sudo -n ip link set ...' — ensure the sudoers rule below is in place:
#   ubuntu ALL=(ALL) NOPASSWD: /sbin/ip link set can* *
info "Interface '${IFACE}' is ready. CAN FD params will be applied by the ROS node."
