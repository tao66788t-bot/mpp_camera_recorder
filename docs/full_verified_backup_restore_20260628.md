# Full Verified Backup And Restore Guide (2026-06-28)

## Scope

This baseline is the current strongest rollback point in the project.

It covers the version where the following were all verified together:

- LCD preview works
- MP4 recording works
- MP4 contains H.265 video and AAC audio
- RTSP video works
- RTSP audio works

## What Was Verified This Time

The current firmware was re-run on the board with the documented clean flow:

1. reboot
2. wait for ADB reconnect
3. push firmware again
4. start only one instance
5. record a short sample
6. stop with `SIGTERM`

Verification evidence:

- board-side `cam_1.mp4` grew normally during recording
- the sample file was pulled back and saved locally
- container inspection showed:
  - H.265 video: `1920x1088`
  - AAC audio: `16000 Hz`, `mono`

## Backup Inventory

### Windows local source snapshot

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260628_full_verified`

### Windows local zip snapshot

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260628_full_verified.zip`

### Windows local binary backup

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_ipc_firmware_full_verified_20260628`

### Windows local MP4 sample

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\sample_mp4_full_verified_20260628.mp4`

### Ubuntu source snapshot mirror

`/home/ubuntu/backup_20260628_full_verified`

### Ubuntu zip mirror

`/home/ubuntu/backup_20260628_full_verified.zip`

### Ubuntu binary backup

`/home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_full_verified_20260628`

### Board-side binary backup

`/mnt/UDISK/backup_ipc_firmware_full_verified_20260628`

## Recommended Restore Strategy

### Case 1: Source code got changed badly

Use the source snapshot first.

Run:

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\restore_full_verified_20260628.ps1`

Then rebuild and redeploy on Ubuntu with the standard flow.

This is the safest restore path.

### Case 2: Source code is fine, but board runtime is broken

Use the verified binary directly.

On Ubuntu:

```bash
cp /home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_full_verified_20260628 \
   /home/ubuntu/mpp_camera_recorder/ipc_firmware

cd /home/ubuntu/mpp_camera_recorder
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
adb shell "ps | grep ipc"
```

### Case 3: Windows project directory was damaged

Restore in this order:

1. `backup_20260628_full_verified`
2. if that is damaged too, use `backup_20260628_full_verified.zip`
3. if local copies are gone, copy back from `/home/ubuntu/backup_20260628_full_verified`

## Why This Backup Is Safer Than Earlier Ones

Compared with the earlier baselines, this one adds all of the following at once:

- LCD verified after the LCD recovery work
- RTSP audio already reconnected
- MP4 audio already verified
- a real sample MP4 artifact saved locally
- copies stored on Windows, Ubuntu, and board UDISK

So this is the best rollback point before any next-stage feature work.

## Rules After Today

1. Do not edit anything inside `backup_20260628_full_verified`.
2. Do not rename or overwrite `backup_ipc_firmware_full_verified_20260628`.
3. If LCD/RTSP/audio starts behaving strangely again, restore this baseline first.
4. Do not trust board behavior without a full `reboot -> reconnect -> re-push -> single-instance start` cycle.

## Fast Path To View The Saved MP4

Open:

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\sample_mp4_full_verified_20260628.mp4`

This is the sample pulled from the board after the current full-path verification.
