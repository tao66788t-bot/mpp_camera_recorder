#!/bin/sh
set -e

echo '=== adb devices ==='
timeout 8 adb devices || true

echo '=== ubuntu ipc_firmware ==='
ls -l /home/ubuntu/mpp_camera_recorder/ipc_firmware || true
md5sum /home/ubuntu/mpp_camera_recorder/ipc_firmware || true

echo '=== board ipc_firmware ==='
timeout 8 adb shell ls -l /mnt/UDISK/ipc_firmware || true
timeout 8 adb shell md5sum /mnt/UDISK/ipc_firmware || true

echo '=== board isp ctx ==='
timeout 8 adb shell ls -l /mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin || true

echo '=== board ipc process ==='
timeout 8 adb shell "ps | grep ipc" || true
