#!/bin/sh
set -e

cd /home/ubuntu/mpp_camera_recorder
cp backup_ipc_firmware_full_verified_20260628 ipc_firmware
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
sleep 4
adb shell "ps | grep ipc"
