/*
 * winutf8.h - 让 OutGuess 各命令行工具在 Windows 上正确处理 UTF-8（中文/特殊）路径与参数。
 *
 * 背景：MinGW-w64 的 C 运行时默认用系统 ANSI 代码页（CP_ACP，中文 Windows = GBK）来
 * 解码命令行 argv，并用同样的代码页解释 fopen()/CreateProcessA() 等接收的 char* 路径。
 * 这导致：① 非 ASCII 的命令行参数（如中文文件名、-k 密钥）在非中文 Windows 上被破坏；
 *         ② 即便在中文 Windows 上，UTF-8 字节也会被当作 GBK 误解码。
 *
 * 解决办法：在 Windows 构建下，本头提供一组"UTF-8 安全"的包装：
 *   - win32_to_utf8_args()：用 GetCommandLineW + CommandLineToArgvW 取得 UTF-16 命令行，
 *     再统一转成 UTF-8 字节回填 argc/argv；
 *   - fopen_utf8() / open_utf8()：把 UTF-8 路径转 UTF-16，调用 _wfopen / _wopen；
 *   - module_path_utf8() / path_exists_utf8() / path_is_dir_utf8() / create_dir_utf8()：
 *     文件/目录属性与创建；
 *   - find_first_utf8() / find_next_utf8() / find_name_utf8()：目录遍历；
 *   - run_process_utf8()：以隐藏窗口启动一条 UTF-8 命令行（含 exe 路径与参数）。
 * 非 Windows 构建下，这些只是对 POSIX 接口的薄封装（argv 无需处理）。
 *
 * 所有函数均为 static，可安全地被多个翻译单元包含（每个 TU 各一份，不会链接冲突）。
 */

#ifndef WINUTF8_H
#define WINUTF8_H

#include <stdio.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <io.h>

/* 用 UTF-16 命令行重建 UTF-8 argv，避免被按 ANSI 代码页误解码 */
static void win32_to_utf8_args(int *argc, char ***argv)
{
	int wargc = 0;
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
	if (wargv == NULL || wargc <= 0)
		return;

	char **uargv = (char **)malloc(sizeof(char *) * (size_t)(wargc + 1));
	if (uargv == NULL) {
		LocalFree(wargv);
		return;
	}

	int i;
	for (i = 0; i < wargc; i++) {
		int need = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
			NULL, 0, NULL, NULL);
		if (need <= 0) {
			uargv[i] = (char *)malloc(1);
			if (uargv[i])
				uargv[i][0] = '\0';
			continue;
		}
		uargv[i] = (char *)malloc((size_t)need);
		if (uargv[i] == NULL) {
			uargv[i] = (char *)malloc(1);
			if (uargv[i])
				uargv[i][0] = '\0';
			continue;
		}
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
			uargv[i], need, NULL, NULL);
	}
	uargv[wargc] = NULL;
	LocalFree(wargv);

	*argc = wargc;
	*argv = uargv;
}

/* UTF-8 路径安全的 fopen（Windows 上转 UTF-16 调 _wfopen） */
static FILE *fopen_utf8(const char *path, const char *mode)
{
	wchar_t wpath[4096];
	wchar_t wmode[16];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 4096) == 0)
		return NULL;
	if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16) == 0)
		return NULL;
	return _wfopen(wpath, wmode);
}

/* UTF-8 路径安全的 open（Windows 上转 UTF-16 调 _wopen） */
static int open_utf8(const char *path, int flags, int mode)
{
	wchar_t wpath[4096];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 4096) == 0)
		return -1;
	return _wopen(wpath, flags, mode);
}

/* 取当前模块（exe）路径，写入 UTF-8 到 buf，返回字节长度（不含结尾），失败返回 -1 */
static int module_path_utf8(char *buf, size_t size)
{
	wchar_t w[MAX_PATH];
	DWORD n = GetModuleFileNameW(NULL, w, MAX_PATH);
	if (n == 0 || n >= MAX_PATH)
		return -1;
	int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, (int)size, NULL, NULL);
	return need > 0 ? need - 1 : -1;
}

static int path_attributes_utf8(const char *path)
{
	wchar_t w[MAX_PATH];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH) == 0)
		return -1;
	DWORD a = GetFileAttributesW(w);
	if (a == INVALID_FILE_ATTRIBUTES)
		return -1;
	return (int)a;
}

static int path_exists_utf8(const char *path)
{
	return path_attributes_utf8(path) != -1;
}

static int path_is_dir_utf8(const char *path)
{
	int a = path_attributes_utf8(path);
	return a != -1 && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int create_dir_utf8(const char *path)
{
	wchar_t w[MAX_PATH];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH) == 0)
		return 0;
	if (CreateDirectoryW(w, NULL))
		return 1;
	return GetLastError() == ERROR_ALREADY_EXISTS;
}

static HANDLE find_first_utf8(const char *pattern, WIN32_FIND_DATAW *data)
{
	wchar_t w[MAX_PATH];
	if (MultiByteToWideChar(CP_UTF8, 0, pattern, -1, w, MAX_PATH) == 0)
		return INVALID_HANDLE_VALUE;
	return FindFirstFileW(w, data);
}

static BOOL find_next_utf8(HANDLE h, WIN32_FIND_DATAW *data)
{
	return FindNextFileW(h, data);
}

/* 把 WIN32_FIND_DATAW 里的文件名转成 UTF-8 写入 buf，返回字节长度，失败返回 -1 */
static int find_name_utf8(const WIN32_FIND_DATAW *data, char *buf, size_t size)
{
	int need = WideCharToMultiByte(CP_UTF8, 0, data->cFileName, -1,
		buf, (int)size, NULL, NULL);
	return need > 0 ? need - 1 : -1;
}

/* 以隐藏窗口启动一条 UTF-8 命令行（整条含 exe 路径与参数），返回退出码，失败返回 -1 */
static int run_process_utf8(const char *commandUtf8)
{
	wchar_t wcmd[32768];
	if (MultiByteToWideChar(CP_UTF8, 0, commandUtf8, -1, wcmd, 32768) == 0)
		return -1;

	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
			NULL, NULL, &si, &pi))
		return -1;

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return (int)code;
}

#else /* !_WIN32 —— 非 Windows：直接走 POSIX，argv 无需处理 */

static void win32_to_utf8_args(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
}

#define fopen_utf8 fopen

#include <fcntl.h>
#include <sys/stat.h>
static int open_utf8(const char *path, int flags, int mode)
{
	return open(path, flags, mode);
}

#endif /* _WIN32 */

#endif /* WINUTF8_H */
