# ============================================================
#  IPC Firmware v3.0 — Makefile
#  VI + Audio + VENC + MUX + RTSP 完整编译
# ============================================================
# 用法:
#   make              # 交叉编译
#   make clean        # 清理
#   make help         # 查看帮助
# ============================================================

# ---- 编译器 ----
TOOLCHAIN_DIR = /home/ubuntu/tina-v853-100ask/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain
CROSS_COMPILE = $(TOOLCHAIN_DIR)/bin/arm-openwrt-linux-muslgnueabi-
CC            = $(CROSS_COMPILE)gcc
CXX           = $(CROSS_COMPILE)g++

# ---- 目录 ----
SRCDIR   := src
INCDIR   := include
BUILDDIR := build
TARGET   := ipc_firmware

# ---- 源文件 ----
SRCS_C   := $(wildcard $(SRCDIR)/*.c)
SRCS_CPP := $(wildcard $(SRCDIR)/*.cpp)
OBJS_C   := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS_C))
OBJS_CPP := $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(SRCS_CPP))
OBJS     := $(OBJS_C) $(OBJS_CPP)

# ---- MPP SDK 路径 ----
STAGING_DIR ?= /home/ubuntu/tina-v853-100ask/out/v853-100ask/staging_dir
SYSROOT     = $(STAGING_DIR)/target
SDK_ODET_DIR := /home/ubuntu/tina-v853-100ask/external/eyesee-mpp/middleware/sun8iw21/sample/sample_odet_demo
SDK_ODET_POST_DIR := $(SDK_ODET_DIR)/post
SDK_ODET_NBG_DIR := $(SDK_ODET_DIR)/yolov3-tiny_nbg_viplite
SDK_VIPLITE_INC_DIR := $(SYSROOT)/usr/include/viplite-driver
SDK_AWNN_INC_DIR := $(SYSROOT)/usr/include/libawnn

# 头文件路径（自动发现 eyesee-mpp 下所有子目录 + 通用路径）
MPP_INC  := -I$(SYSROOT)/usr/include
MPP_INC  += $(shell find $(SYSROOT)/usr/include/eyesee-mpp -type d \
              -not -path "*/external/jsoncpp*" \
              -not -path "*/external/SQLiteCpp*" \
              -printf '-I%p ' 2>/dev/null)
MPP_INC  += -I$(SDK_ODET_DIR) -I$(SDK_ODET_POST_DIR) -I$(SDK_ODET_NBG_DIR) -I$(SDK_VIPLITE_INC_DIR) -I$(SDK_AWNN_INC_DIR)

# 库路径
MPP_LIB  := -L$(SYSROOT)/usr/lib
MPP_LIB  += -L$(SYSROOT)/usr/lib/eyesee-mpp
MPP_LIB  += -L$(SYSROOT)/lib

# ---- 编译选项 ----
CFLAGS  := -Wall -Wextra -std=gnu11 \
           -I$(INCDIR)              \
           $(MPP_INC)               \
           -DISP_RUN=1
CXXFLAGS := -Wall -Wextra -std=gnu++11 \
            -I$(INCDIR)              \
            $(MPP_INC)               \
            -DISP_RUN=1

# ---- 链接 ----
# 自动收集 eyesee-mpp 下所有 .a 文件
MPP_ALL_LIBS := $(wildcard $(SYSROOT)/usr/lib/eyesee-mpp/*.a)
MPP_LIBS_A   := $(patsubst $(SYSROOT)/usr/lib/eyesee-mpp/lib%.a,-l%,$(MPP_ALL_LIBS))

LDFLAGS := --sysroot=$(SYSROOT) \
           $(MPP_LIB)     \
           -Wl,-Bstatic \
           -Wl,--start-group \
           $(MPP_LIBS_A)     \
           -Wl,--end-group   \
           -lz              \
           -Wl,-Bdynamic    \
           -lstdc++         \
           -lawnn_full      \
           -lVIPlite        \
           -lasound         \
           -lpthread        \
           -lm              \
           -lrt             \
           -ldl

# ---- 默认目标 ----
.PHONY: all
all: CFLAGS += -O2 -DNDEBUG
all: $(TARGET)

# ---- 调试版本 ----
.PHONY: debug
debug: CFLAGS += -g -O0 -DDEBUG
debug: $(TARGET)

# ---- 链接 ----
$(TARGET): $(OBJS)
	@echo "  LD    $@"
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "========================================="
	@echo "  BUILD $(TARGET) OK"
	@echo "========================================="
	@echo ""
	@echo "  Copy to board: scp $(TARGET) root@<board_ip>:/usr/bin/"
	@echo "  Run on board:  ./$(TARGET)"
	@echo "  Play:          ffplay rtsp://<board_ip>:554/stream"
	@echo ""

# ---- 编译 .c → .o ----
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	@echo "  CXX   $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# ---- 清理 ----
.PHONY: clean
clean:
	@rm -rf $(BUILDDIR) $(TARGET)
	@echo "  CLEAN done"

# ---- 帮助 ----
.PHONY: help
help:
	@echo "============================================"
	@echo "  IPC Firmware v3.0 Build System"
	@echo "============================================"
	@echo ""
	@echo "  make          Release build ($(TARGET))"
	@echo "  make debug    Debug build with -g -O0"
	@echo "  make clean    Remove build artifacts"
	@echo ""
	@echo "  Modules:"
	@echo "    vi_module    — OV5640 -> MIPI -> VI -> ISP"
	@echo "    audio_module — MIC -> AI -> AENC (AAC-LC)"
	@echo "    venc_module  — H.265 hardware encode"
	@echo "    mux_module   — MP4 mux (video + audio)"
	@echo "    rtsp_module  — RTSP server + RTP FU-A push"
	@echo "    stream_router — VENC/AENC GetStream -> MUX + RTSP"
	@echo ""
