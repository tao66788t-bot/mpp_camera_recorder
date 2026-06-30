/**
 * @file    stubs.c
 * @brief   为 SDK 静态库提供缺失的日志函数符号
 *          同时让所有日志原样输出到 stdout，方便调试
 */
#include <stdio.h>
#include <stdarg.h>

void GLOG_PRINT(int level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%d] ", level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void log_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
