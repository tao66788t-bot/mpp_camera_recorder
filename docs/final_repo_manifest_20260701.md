# 最终仓库整理清单（2026-07-01）

## 1. 这次整理的目标

这次不是把工作目录里所有文件一股脑上传，而是整理出一份适合长期保留在 GitHub 上的“最终可复现版本”。

原则：

- 保留最终源码
- 保留长期有价值的验证脚本
- 保留项目说明和过程沉淀文档
- 不把明显的运行产物、缓存目录、临时垃圾文件塞进仓库

## 2. 本次纳入 Git 的内容

### 2.1 核心源码

- `src/`
- `include/`
- `Makefile`

其中这次明确属于最终版本的关键源码变化包括：

- `src/main.c`
  - VI2 预览链像素格式切换为 `MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420`
  - 保留说明注释，明确只动 LCD 预览链，不动 VI0 录像主链
- `src/diag_vi_dump.c`
  - 诊断抓帧配置与当前主链路保持一致，方便排查 VI0/VI2 原始帧

### 2.2 文档

- 根目录 `README.md`
- `docs/` 下的适配记录、验证记录、面试沉淀和最终使用说明

特别重要的新增总览文档：

- `docs/final_release_usage_20260701.md`
- `docs/code_architecture_20260701.md`
- `docs/final_repo_manifest_20260701.md`

### 2.3 可复用脚本

保留这些长期可用的脚本：

- `check_runtime_status_remote.sh`
- `deploy_uvswap_preview_remote.sh`
- `restore_face_verified_remote.sh`
- `start_uvswap_current_binary_remote.sh`
- `docs/start_rtsp_vlc_windows.ps1`

### 2.4 分析与验证工具

保留 `tools/` 目录中的脚本与模板，因为它们对应：

- 音频质量分析
- A/V 同步分析
- 色彩与 LCD 一致性分析
- 路由日志统计
- 合成测试素材生成
- 简历/面试指标自动检查

这些工具虽然不是固件运行必需，但属于项目工程化资产，适合留在仓库。

### 2.5 测试资产

保留 `assets/` 目录：

- 颜色卡
- AV Sync 测试图
- 音频测试波形

它们是生成验证结论的输入，不是运行时垃圾文件。

## 3. 本次不纳入 Git 的内容

### 3.1 运行产物

不纳入：

- `artifacts/`
- 运行过程中生成的 `.mp4`
- 抓出的 `.png`
- 抓出的 `.yuv`
- `.log`

原因：

- 这些是结果样本，不是程序复现所必需
- 体积会不断膨胀
- 更适合本地保存或单独打包归档

### 3.2 缓存和临时文件

不纳入：

- `__pycache__/`
- `*.pyc`
- `tmp_*.py`

### 3.3 本地大备份

不纳入：

- `backup_*`
- `backup_*/`
- `.zip`

原因：

- 这些已经在本地硬盘保留
- Git 仓库里保留源码和说明即可

## 4. 不在 Git 里但必须知道的外部依赖

真正复现这个项目，还需要下面两个不应直接依赖 Git 重新生成的文件：

- `Facedet_480_288_nv12.nb`
- `isp0_1920_1088_20_0_ctx_saved.bin`

说明：

- `.nb` 是 NPU 模型包，不是 `make` 产物
- ISP `.bin` 是板端运行时 context，也不是 `make` 产物

所以 GitHub 上的源码仓库解决的是“代码可追踪、文档可追踪、流程可追踪”，不是替代所有板端二进制和运行态文件。

## 5. 这次整理后的使用建议

### 5.1 看代码入口

先看：

- `README.md`
- `docs/final_release_usage_20260701.md`
- `docs/code_architecture_20260701.md`

### 5.2 要跑程序

按 `README.md` 里的部署流程执行。

### 5.3 要完整回滚

除了 GitHub 仓库，还要单独保存：

- `backup_20260701_all/`
- `backup_ipc_firmware_lcd_recovered_20260701`
- `backup_Facedet_480_288_nv12_20260701.nb`
- `backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`

## 6. 一句话结论

这次仓库整理的目标不是“把所有东西都传上去”，而是把**真正能解释项目、复盘项目、继续开发项目**的那一层留下来，把**运行垃圾和本地大备份**留在仓库外。 
