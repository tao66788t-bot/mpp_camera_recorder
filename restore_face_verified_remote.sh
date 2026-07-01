#!/bin/sh
set -e

cd /home/ubuntu/mpp_camera_recorder

cp backup_ipc_firmware_face_verified_20260630 ipc_firmware

adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb

adb shell reboot
sleep 15

adb kill-server || true
adb start-server
sleep 2
adb devices

adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb

adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb" &
sleep 4
adb shell "ps | grep ipc" || true
