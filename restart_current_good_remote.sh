#!/bin/sh
set -e
adb devices
adb -s 20080411 shell "ps | grep ipc" || true
adb -s 20080411 shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
sleep 4
adb -s 20080411 shell "ps | grep ipc"