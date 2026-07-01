# IPC 项目备份与回滚说明

## 1. 这份备份是干什么的

这份备份的目标只有一个：

- 当 LCD 预览、录像链路或者当前工程代码被改坏时，可以快速恢复到 2026-06-27 这版“LCD 正常、MP4 正常”的工作状态。

它不是归档摆设，而是后续开发中的“回滚基线”。

---

## 2. 当前有哪些备份

### 2.1 源码备份目录

路径：

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260627_lcd_fixed`

里面保存的是：

- `src/` 对应的 `.c` 文件快照
- `include/` 对应的 `.h` 文件快照
- `Makefile`
- 备份说明 `README.md`

用途：

- 当前工程源码被改坏时，用它恢复源码

### 2.2 可工作二进制备份

路径：

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_ipc_firmware_lcd_fixed_20260627`

用途：

- 不想重新编译，只想把板子迅速恢复到可工作状态时，直接拿它部署

### 2.3 当前工程目录

路径：

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder`

说明：

- 这是“正在开发”的目录
- 这里的文件未来会继续变化
- 不要把它当成永久安全版本

---

## 3. 最推荐的恢复策略

### 3.1 只是板子上的程序坏了

例如：

- 板子上运行的是旧程序
- 推送错了二进制
- reboot 后 LCD 又不对了

这种情况优先恢复二进制，不要先动源码。

### 3.2 当前源码被改乱了

例如：

- 改了 `main.c`、`vi_module.c`、`vo_module.c` 后效果变差
- 不确定自己现在的工程里混入了哪些试验代码

这种情况优先恢复源码备份，再重新编译。

### 3.3 源码和板子状态都不可信

例如：

- 当前工程文件改乱了
- 板子上也跑着不确定版本

这种情况按下面顺序恢复：

1. 恢复源码备份
2. 在 Ubuntu 重新编译
3. 用新编译出来的程序重新部署到板子

---

## 4. 源码回滚怎么做

### 4.1 回滚前先保留现场

不要直接覆盖当前工程，先把当前现场另存一份。

建议做法：

- 把 `src/`、`include/`、`Makefile` 单独复制到一个新目录
- 目录名带上日期，例如：`temp_before_restore_20260627_xx`

这样即使回滚后发现还想参考某个临时改动，也还能找回来。

### 4.2 用备份覆盖当前工程

Windows PowerShell：

```powershell
$proj = "D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder"
$bak  = "$proj\backup_20260627_lcd_fixed"

Copy-Item "$bak\*.c" "$proj\src\" -Force
Copy-Item "$bak\*.h" "$proj\include\" -Force
Copy-Item "$bak\Makefile" "$proj\Makefile" -Force
```

这一步做完后，当前工程就回到了这份备份对应的源码状态。

---

## 5. 板子程序怎么恢复

### 5.1 最快恢复方式：直接恢复可工作二进制

如果你不想重新编译，只想先把板子拉回正常状态，可以直接使用：

`backup_ipc_firmware_lcd_fixed_20260627`

先把它复制或改名成 `ipc_firmware`，再部署到板子。

### 5.2 固定部署步骤

在 Ubuntu 编译机执行：

```bash
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
adb shell "ps | grep ipc_firmware"
```

注意：

- reboot 后 `UDISK` 可能不稳定，所以通常要补一次 `adb push`
- 固件只能启动一次，避免重复占用 `video0`

---

## 6. 如何判断恢复成功

至少确认下面四项：

1. `adb devices` 中板子状态为 `20080411 device`
2. `adb shell "ps | grep ipc_firmware"` 能看到唯一的 `./ipc_firmware`
3. LCD 能正常显示 480x800 预览
4. MP4 录制仍正常生成

如果只有 1、2 成功，但 LCD 仍异常，说明是“程序起来了，但显示链路仍有问题”，不能算恢复完成。

---

## 7. 如何避免以后误删

建议你把备份至少保留三份：

### 7.1 当前 Windows 工作区保留一份

保留：

- `backup_20260627_lcd_fixed`
- `backup_ipc_firmware_lcd_fixed_20260627`

### 7.2 Ubuntu 编译机再保留一份

建议在 Ubuntu 上单独建目录，例如：

`/home/ubuntu/mpp_camera_recorder_backup_20260627_lcd_fixed/`

用途：

- 即使 Windows 侧误删，还能从 Ubuntu 找回

### 7.3 网盘或压缩包再存一份

建议把下面两个对象单独打包上传：

- 源码备份目录
- 二进制备份文件

这样即使本机目录被误删，也还有离线副本。

---

## 8. 回滚时最容易踩的坑

### 8.1 只恢复了源码，没有重新编译部署

源码恢复不代表板子已经恢复，板子仍然跑的是旧程序。

### 8.2 只 push 了程序，没有 reboot

这个项目里 ISP/VE 硬件状态会残留，很多问题不 reboot 很难真正回到干净状态。

### 8.3 重复启动固件

重复启动会导致设备节点被占用，LCD 甚至会直接表现异常。

### 8.4 把备份目录当开发目录继续改

备份目录只能当“恢复基线”，不要在里面继续开发，否则它也会失去备份意义。

---

## 9. 推荐执行原则

以后只要遇到“LCD 又不正常了”，建议先按这个顺序判断：

1. 当前板子是不是只启动了一个固件实例
2. 当前板子跑的是不是这份可工作版本
3. 当前源码是否已经偏离备份版本
4. 如果不确定，优先回到备份基线，不要继续在不可信状态上叠加调试

---

## 10. 一句话说明

这份备份不是为了留档，而是为了保证后面任何一次调试失手后，都能把项目快速拉回“LCD 正常、MP4 正常”的已知工作状态。
