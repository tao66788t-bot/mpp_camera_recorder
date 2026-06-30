#!/bin/sh
set -e
adb -s 20080411 shell 'rm -f /mnt/UDISK/diag_vi0.yuv /mnt/UDISK/diag_vi2.yuv'
adb -s 20080411 shell 'cd /mnt/UDISK && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --diag-vi-dump'
adb -s 20080411 pull /mnt/UDISK/diag_vi0.yuv /home/ubuntu/mpp_camera_recorder/diag_vi0_new.yuv
adb -s 20080411 pull /mnt/UDISK/diag_vi2.yuv /home/ubuntu/mpp_camera_recorder/diag_vi2_new.yuv
ls -l /home/ubuntu/mpp_camera_recorder/diag_vi0_new.yuv /home/ubuntu/mpp_camera_recorder/diag_vi2_new.yuv