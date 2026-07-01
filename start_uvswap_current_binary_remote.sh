#!/bin/sh
set -e

adb devices
adb shell "ps | grep ipc" || true
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb" &
sleep 4
adb shell "ps | grep ipc" || true
