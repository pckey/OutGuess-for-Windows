/*
 * compat.h - Windows 兼容层，用于将 OutGuess（原本为 Unix/BSD 编写）
 * 移植到原生 Windows，使用 MinGW-w64 编译。
 *
 * 仅当定义 _WIN32 时才包含本文件；在 Linux/macOS 上，
 * 构建仍使用原版 POSIX 头文件。
 *
 * 针对 Windows 目标进行的替换：
 *   - <unistd.h> / getopt()      -> 自带的 getopt() 实现
 *   - <netinet/in.h> htonl/ntohl -> 不依赖 winsock 的字节序交换宏
 *                                    （避免需要 -lws2_32）
 *   - <sys/mman.h>               -> 未使用（HAVE_MMAP 保持未定义，
 *                                    outguess.c 自身会回退到
 *                                    基于 read() 的实现）
 *   - u_char/u_int/u_short       -> MSVCRT 未定义的 BSD 类型
 *   - O_BINARY                   -> 以二进制方式打开 JPEG/PNM，
 *                                    避免 CRT 的 CRLF 转换
 */
#ifndef OUTGUESS_WIN32_COMPAT_H
#define OUTGUESS_WIN32_COMPAT_H

#ifdef _WIN32

#include <io.h>
#include <string.h>

/* ---- 原代码全局使用的 BSD 风格类型 ---- */
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;

/* ---- u_intNN_t（BSD/glibc 的 sys/types.h 中定义，MSVCRT 缺失）---- */
#include <stdint.h>
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

/* ---- strcasecmp / strncasecmp（MSVCRT 仅提供带下划线前缀的版本）---- */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/* ---- htonl/ntohl，无需链接 Winsock ---- */
#ifndef htonl
static __inline unsigned long outguess_htonl(unsigned long x)
{
	return ((x & 0x000000ffUL) << 24) |
	       ((x & 0x0000ff00UL) << 8)  |
	       ((x & 0x00ff0000UL) >> 8)  |
	       ((x & 0xff000000UL) >> 24);
}
#define htonl(x) outguess_htonl(x)
#define ntohl(x) outguess_htonl(x)
#endif

/* ---- 二进制方式打开文件：避免 CRT 进行 CRLF 转换 ---- */
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

/* ---- 极简 getopt() 实现（MinGW-w64 默认不包含 getopt）---- */
extern char *optarg;
extern int   optind, opterr, optopt;
int outguess_getopt(int argc, char * const argv[], const char *optstring);
#define getopt outguess_getopt

#endif /* _WIN32 */

#endif /* OUTGUESS_WIN32_COMPAT_H */
