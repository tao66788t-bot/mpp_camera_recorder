#!/bin/sh
set -eu

adb shell 'killall ipc_firmware 2>/dev/null || true'
adb shell 'cd /mnt/UDISK || exit 90; chmod +x ipc_firmware || exit 91; export LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib; nohup /mnt/UDISK/ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb >/tmp/ipc_face.log 2>&1 </dev/null &'
sleep 2
adb shell 'ps | grep ipc_firmware'
adb shell 'tail -n 80 /tmp/ipc_face.log'
