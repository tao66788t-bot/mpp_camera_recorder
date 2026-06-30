# RTSP Windows Viewing Guide (2026-06-28)

## Conclusion

The RTSP server in `ipc_firmware` is already pushing valid H.265 video.

Evidence verified on 2026-06-28:

- Board process is running and listening on `0.0.0.0:554`
- The router thread is continuously receiving `VENC_GetStream()` frames
- After RTSP `PLAY`, the server immediately sends interleaved RTP data
- The first H.265 packets observed on the wire are:
  - `VPS (type 32)`
  - `SPS (type 33)`
  - `PPS (type 34)`
  - `IDR (type 19)`
- Windows VLC successfully decoded the stream and logged `Received first picture`

So if VLC shows only a black cone, the first thing to check is the access path, not the firmware pipeline.

## Why Direct `rtsp://192.168.x.x:554/stream` Failed

During verification, the board network state was:

- `eth0`: `DOWN`
- `wlan0`: `DOWN`
- `wlan1`: `DOWN`

That means the board did not actually own a usable LAN/Wi-Fi IP at that moment.

The reliable path is:

1. Board RTSP server listens on `554`
2. Ubuntu VM uses `adb forward tcp:8554 tcp:554`
3. Windows uses SSH local port forwarding to Ubuntu
4. VLC opens `rtsp://127.0.0.1:8554/stream`

## Recommended Windows Path

### Step 1. On Ubuntu VM

Run:

```bash
adb forward tcp:8554 tcp:554
adb forward --list
```

Expected output contains:

```text
20080411 tcp:8554 tcp:554
```

### Step 2. On Windows

Open an SSH tunnel to the Ubuntu VM:

```bash
ssh -o StrictHostKeyChecking=accept-new -L 8554:127.0.0.1:8554 ubuntu@192.168.73.135
```

Keep this window open while watching RTSP.

### Step 3. In VLC

Open:

```text
rtsp://127.0.0.1:8554/stream
```

Important:

- Prefer forcing RTSP over TCP
- Otherwise VLC may first try UDP, wait about 10 seconds, then switch back to TCP
- That delay can look like "connected but black screen"

The most stable launch form is:

```bash
"C:\Program Files\VideoLAN\VLC\vlc.exe" --rtsp-tcp rtsp://127.0.0.1:8554/stream
```

## One-Click Script

Use the script:

- `docs/start_rtsp_vlc_windows.ps1`

What it does:

1. Opens SSH local forwarding from Windows to Ubuntu
2. Runs `adb forward tcp:8554 tcp:554` on Ubuntu
3. Launches VLC with `--rtsp-tcp`

## Extra Notes

- If the board is later connected to Wi-Fi successfully, then direct LAN viewing can use:

```text
rtsp://<board_ip>:554/stream
```

- But for the current environment, the forwarded path is the verified working path.

- This check was validated with Windows VLC 3.x. The log showed:
  - RTSP handshake succeeded
  - H.265 decoder started
  - `Received first picture`
