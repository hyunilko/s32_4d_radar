#!/usr/bin/env bash
set -euo pipefail

LOG_LINES=200

echo "=== [1] stop any user apps using /dev/pcan* (best-effort) ==="
sudo fuser -v /dev/pcan* 2>/dev/null || true

echo "=== [2] unload conflicting kernel driver (SocketCAN peak_usb) if present ==="
if lsmod | grep -q '^peak_usb'; then
  echo "[INFO] unloading peak_usb (possible conflict with pcan chardev)"
  sudo modprobe -r peak_usb || true
fi

echo "=== [3] unload pcan module stack ==="
# unload pcan and common dependencies if loaded
for m in pcan; do
  if lsmod | grep -q "^${m}\b"; then
    sudo modprobe -r "${m}" || sudo rmmod "${m}" || true
  fi
done

echo "=== [4] ask user to replug device on USB 2.0 port ==="
echo ">> PCAN-USB FD를 뽑으세요 (LED 꺼질 때까지)"
read -r -p "   뽑았으면 Enter... " _
echo ">> USB 2.0(검은색) 포트에 다시 꽂으세요 (허브/연장선 X)"
read -r -p "   꽂았으면 Enter... " _

echo "=== [5] disable USB autosuspend for PEAK (idVendor=0c72) - best-effort ==="
# apply for all matching devices currently enumerated
for dev in /sys/bus/usb/devices/*; do
  if [[ -f "$dev/idVendor" ]] && grep -qi '^0c72$' "$dev/idVendor"; then
    if [[ -f "$dev/power/control" ]]; then
      echo on | sudo tee "$dev/power/control" >/dev/null || true
      echo "[INFO] autosuspend off: $dev"
    fi
  fi
done

echo "=== [6] reload pcan driver ==="
sudo modprobe pcan

echo "=== [7] quick status ==="
echo "--- lsmod ---"
lsmod | egrep '^(pcan|peak_usb)\b' || true

echo "--- /dev/pcan* ---"
ls -l /dev/pcan* 2>/dev/null || true

echo "--- /proc/pcan ---"
cat /proc/pcan 2>/dev/null || true

echo "=== [8] dmesg (last ${LOG_LINES} lines, filtered) ==="
sudo dmesg | tail -n "${LOG_LINES}" | egrep -i 'pcan|usb|xhc|timeout|protocol|reset' || true

echo "DONE."
