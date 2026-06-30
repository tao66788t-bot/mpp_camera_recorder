#!/bin/sh
set -e
adb -s 20080411 shell 'ps | grep ipc || true'
adb -s 20080411 shell 'cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware >/dev/null 2>&1 </dev/null &'
sleep 2
adb -s 20080411 shell 'ps | grep ipc'