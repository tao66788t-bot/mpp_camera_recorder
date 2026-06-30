#!/bin/bash
set -euo pipefail

duration="${1:-25}"

cd /home/ubuntu/mpp_camera_recorder
rm -f qp_foreground_console.log qp_graceful.mp4

adb shell reboot || true
sleep 15
adb kill-server >/dev/null 2>&1 || true
adb start-server >/dev/null 2>&1 || true
sleep 2
adb devices

adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell 'rm -f /mnt/UDISK/cam_0.mp4 /mnt/UDISK/cam_1.mp4'

(
    sleep "$duration"
    adb shell 'pidof ipc_firmware || ps | grep ipc' || true
    adb shell 'kill -TERM $(pidof ipc_firmware)' 2>/dev/null \
        || adb shell 'killall -TERM ipc_firmware' 2>/dev/null \
        || true
) &
killer_pid=$!

adb shell "cd /mnt/UDISK && sh run_ipc_once.sh" > qp_foreground_console.log 2>&1 || true

wait "$killer_pid" || true
sleep 3

adb shell 'ps | grep ipc' || true
adb shell 'ls -l /mnt/UDISK/cam_1.mp4'
adb pull /mnt/UDISK/cam_1.mp4 qp_graceful.mp4

ls -l qp_graceful.mp4 qp_foreground_console.log
