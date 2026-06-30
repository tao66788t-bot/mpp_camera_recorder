#!/bin/sh
set -eu

pkill -f "script -q -c adb shell.*ipc_firmware" >/dev/null 2>&1 || true
pkill -f "adb shell.*ipc_firmware" >/dev/null 2>&1 || true
nohup /usr/bin/script -q -c "adb shell 'cd /mnt/UDISK; chmod +x ipc_firmware; LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware'" /tmp/ipc_adb_host.log >/dev/null 2>&1 &
sleep 4
ps -ef | grep 'script -q -c' | grep ipc_firmware || true
ps -ef | grep 'adb shell' | grep ipc_firmware || true
adb shell 'ps | grep ipc'
tail -n 40 /tmp/ipc_adb_host.log || true
