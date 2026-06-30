#!/bin/sh
set -e
adb -s 20080411 shell 'rm -f /mnt/UDISK/ipc_runtime_board.log'
adb -s 20080411 shell 'cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware >/mnt/UDISK/ipc_runtime_board.log 2>&1 </dev/null &'
sleep 3
adb -s 20080411 shell 'ps | grep ipc || true'
echo ---LOG---
adb -s 20080411 shell 'tail -n 120 /mnt/UDISK/ipc_runtime_board.log 2>/dev/null || ls -l /mnt/UDISK/ipc_runtime_board.log 2>/dev/null || echo no_log'