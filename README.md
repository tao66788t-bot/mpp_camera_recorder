# mpp_camera_recorder

这是收敛后的最终仓库版本，只保留：

- 可编译的最终版源码
- 一份总说明文档
- 一个最终部署脚本
- 一个运行状态检查脚本
- 两个无法通过 `make` 生成、但运行时必须用到的外部文件

当前最终版本目标：

- LCD 预览正常
- MP4 录像正常
- RTSP 音视频推流正常
- 人脸检测叠框可用

## 保留目录

- `src/`
- `include/`
- `Makefile`
- `docs/project_guide.md`
- `deploy_final_remote.sh`
- `check_runtime_remote.sh`
- `Facedet_480_288_nv12.nb`
- `isp0_1920_1088_20_0_ctx_saved.bin`

## 快速结论

- `ipc_firmware` 可以从源码编译出来
- `Facedet_480_288_nv12.nb` 不能通过 `make` 生成
- `isp0_1920_1088_20_0_ctx_saved.bin` 不能通过 `make` 生成

## 编译

```bash
cd ~/mpp_camera_recorder
make clean
make
```

## 部署

```bash
sh deploy_final_remote.sh
```

## 说明文档

- [项目总说明](D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\docs\project_guide.md)
