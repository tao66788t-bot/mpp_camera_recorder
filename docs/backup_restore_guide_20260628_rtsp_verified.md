# RTSP Verified Backup And Restore Guide

## Purpose

This document records the backup created on 2026-06-28 after RTSP was verified end-to-end.

The goal is simple:

- preserve a rollback point where LCD, MP4, and RTSP all work
- avoid losing the current good state before audio development continues

## Backup Inventory

### Local source snapshot

Path:

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260628_rtsp_verified`

### Local binary backup

Path:

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_ipc_firmware_rtsp_verified_20260628`

### Ubuntu project snapshot

Path:

`/home/ubuntu/mpp_camera_recorder_backup_20260628_rtsp_verified`

### Ubuntu binary backup

Path:

`/home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_rtsp_verified_20260628`

### Board-side binary backup

Path:

`/mnt/UDISK/backup_ipc_firmware_rtsp_verified_20260628`

## Verified RTSP Result

On 2026-06-28, the following evidence was confirmed:

- RTSP handshake succeeded
- RTSP `PLAY` caused immediate RTP output
- Captured H.265 start sequence was `VPS -> SPS -> PPS -> IDR`
- Windows VLC decoded successfully and reported `Received first picture`

## Why This Baseline Matters

This baseline is stronger than the LCD-only baseline because it proves:

- preview path is healthy
- recording path is healthy
- encoded stream can be consumed externally

That makes it the correct rollback point before adding audio work.

## How To Restore Source Code

PowerShell example:

```powershell
$proj = "D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder"
$bak  = "$proj\backup_20260628_rtsp_verified"

Copy-Item "$bak\*.c" "$proj\src\" -Force
Copy-Item "$bak\*.h" "$proj\include\" -Force
Copy-Item "$bak\Makefile" "$proj\Makefile" -Force
Copy-Item "$bak\rtsp_windows_view_20260628.md" "$proj\docs\" -Force
Copy-Item "$bak\start_rtsp_vlc_windows.ps1" "$proj\docs\" -Force
```

## How To Restore The Binary Quickly

If source is fine and only runtime deployment is broken, restore the binary first.

On Ubuntu:

```bash
cp /home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_rtsp_verified_20260628 \
   /home/ubuntu/mpp_camera_recorder/ipc_firmware

adb push /home/ubuntu/mpp_camera_recorder/ipc_firmware /mnt/UDISK/ipc_firmware
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push /home/ubuntu/mpp_camera_recorder/ipc_firmware /mnt/UDISK/ipc_firmware
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
adb shell "ps | grep ipc"
```

## Windows Viewing Path That Was Verified

Use:

1. Ubuntu:

```bash
adb forward tcp:8554 tcp:554
```

2. Windows:

```bash
ssh -o StrictHostKeyChecking=accept-new -L 8554:127.0.0.1:8554 ubuntu@192.168.73.135
```

3. VLC:

```bash
"C:\Program Files\VideoLAN\VLC\vlc.exe" --rtsp-tcp rtsp://127.0.0.1:8554/stream
```

## Rule Before Further Development

Before touching the audio path:

1. keep this backup intact
2. do not overwrite the backup directory
3. do not delete the saved binary copies
4. if experiments get messy, roll back here first

