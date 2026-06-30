#!/bin/sh
set -e
cd /home/ubuntu/mpp_camera_recorder
adb -s 20080411 shell reboot || true
sleep 15
adb kill-server
adb start-server
sleep 2
adb devices
adb -s 20080411 push ipc_firmware /mnt/UDISK/ipc_firmware
adb -s 20080411 shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
sleep 4
adb -s 20080411 shell "ps | grep ipc"