param(
    [string]$UbuntuHost = "192.168.73.135",
    [string]$UbuntuUser = "ubuntu",
    [int]$LocalPort = 8554,
    [int]$RemotePort = 8554,
    [string]$VlcPath = "C:\Program Files\VideoLAN\VLC\vlc.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $VlcPath)) {
    throw "VLC not found: $VlcPath"
}

$sshExe = "ssh"
$remoteCmd = "adb forward tcp:$RemotePort tcp:554 && adb forward --list && tail -f /dev/null"
$forwardSpec = "$LocalPort`:127.0.0.1:$RemotePort"

Write-Host "[RTSP] Opening SSH tunnel to $UbuntuUser@$UbuntuHost ..."
Write-Host "[RTSP] Tunnel: 127.0.0.1:$LocalPort -> Ubuntu:127.0.0.1:$RemotePort -> board:554"

Start-Process -FilePath $sshExe `
    -ArgumentList @(
        "-o", "StrictHostKeyChecking=accept-new",
        "-L", $forwardSpec,
        "$UbuntuUser@$UbuntuHost",
        $remoteCmd
    )

Start-Sleep -Seconds 2

$url = "rtsp://127.0.0.1:$LocalPort/stream"
Write-Host "[RTSP] Launching VLC: $url"

Start-Process -FilePath $VlcPath `
    -ArgumentList @("--rtsp-tcp", $url)
