#!/bin/sh
set -e
cd /home/ubuntu/mpp_camera_recorder
adb -s 20080411 shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
sleep 3
adb -s 20080411 shell "ps | grep ipc"