# 开机音频静音处理记录（2026-06-28）

## 现象

- 板子每次 reboot 过程中会通过喇叭/LINEOUT 播放声音，夜间调试容易被吓到。

## 排查证据

1. `/etc/init.d/` 启动脚本已完整拉回检查。
2. `/etc/init.d/S03audio` 存在且会在 `rc.final` 中被执行，但原文件大小为 `0` 字节。
3. 板子上 `tinymix controls` 可确认当前音频输出相关控件存在：
   - `DAC volume`
   - `LINEOUT volume`
   - `LINEOUT Switch`
   - `SPK Switch`
4. 当前输出状态曾为：
   - `DAC volume = 160`
   - `LINEOUT volume = 27`
   - `LINEOUT Switch = On`
   - `SPK Switch = On`

## 处理方案

- 不去猜测“哪一个程序在开机时发声”，而是直接利用空的 `S03audio` 启动位，开机早期先把输出静音。
- 对应脚本沉淀在：
  - [S03audio_boot_mute.sh](D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\conf\S03audio_boot_mute.sh)

脚本做的事：

```sh
tinymix set "SPK Switch" 0
tinymix set "LINEOUT Switch" 0
tinymix set "DAC volume" 0
tinymix set "LINEOUT volume" 0
```

## 下发到板子

```sh
scp conf/S03audio_boot_mute.sh ubuntu@192.168.73.135:/tmp/S03audio
ssh ubuntu@192.168.73.135
adb push /tmp/S03audio /etc/init.d/S03audio
adb shell chmod 755 /etc/init.d/S03audio
adb shell /etc/init.d/S03audio start
```

## 恢复声音

如果后面需要重新打开板子喇叭，可手动执行：

```sh
adb shell 'tinymix set "DAC volume" 160'
adb shell 'tinymix set "LINEOUT volume" 27'
adb shell 'tinymix set "LINEOUT Switch" 1'
adb shell 'tinymix set "SPK Switch" 1'
```

如果不再需要开机静音，直接把 `/etc/init.d/S03audio` 改回空文件即可：

```sh
adb shell 'echo -n > /etc/init.d/S03audio'
adb shell chmod 755 /etc/init.d/S03audio
```
