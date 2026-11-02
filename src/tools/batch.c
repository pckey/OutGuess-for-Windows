#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../win32/winutf8.h"
#include "../i18n.h"

static void usage(const char *program)
{
	fprintf(stderr, og_L("Usage:\n", "用法:\n"));
	fprintf(stderr, og_L("  %s embed [-e] [-p quality] [-f] [-k key] -m message -i pattern -o directory\n", "  %s embed [-e] [-p 质量] [-f] [-k 密钥] -m 消息 -i 通配符 -o 目录\n"), program);
	fprintf(stderr, og_L("  %s extract [-e] [-f] [-k key] -i pattern -o directory\n", "  %s extract [-e] [-f] [-k 密钥] -i 通配符 -o 目录\n"), program);
	fprintf(stderr, og_L("  -f overwrite existing output files (default: skip)\n", "  -f 覆盖已存在的输出文件（默认：跳过）\n"));
}

static int get_option(int argc, char **argv, const char *name, char **value)
{
	int i;
	for (i = 2; i + 1 < argc; i++) {
		if (strcmp(argv[i], name) == 0) {
			*value = argv[i + 1];
			return 1;
		}
	}
	return 0;
}

static int has_option(int argc, char **argv, const char *name)
{
	int i;
	for (i = 2; i < argc; i++)
		if (strcmp(argv[i], name) == 0)
			return 1;
	return 0;
}

static int ensure_directory(const char *path)
{
	char parent[MAX_PATH];
	if (path_is_dir_utf8(path))
		return 1;
	strncpy(parent, path, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';
	char *slash = strrchr(parent, '\\');
	if (!slash)
		slash = strrchr(parent, '/');
	if (slash && slash != parent && !(slash == parent + 2 && parent[1] == ':')) {
		*slash = '\0';
		if (!ensure_directory(parent))
			return 0;
	}
	return create_dir_utf8(path);
}

static void quote(char *target, size_t size, const char *value)
{
	snprintf(target, size, "\"%s\"", value);
}

static void basename_from_path(const char *path, char *name, size_t size)
{
	const char *slash = strrchr(path, '\\');
	const char *filename = slash ? slash + 1 : path;
	strncpy(name, filename, size - 1);
	name[size - 1] = '\0';
}

static int process_file(const char *mode, const char *key, const char *message,
	const char *input, const char *outputDirectory, int errorCorrection,
	const char *quality, int force)
{
	char program[MAX_PATH], inputQuoted[MAX_PATH + 2], output[MAX_PATH], outputQuoted[MAX_PATH + 2];
	char keyQuoted[1024], keyArgument[1030], messageQuoted[MAX_PATH + 2];
	char command[4096], filename[MAX_PATH];
	char *slash;
	int pathLength;
	if (module_path_utf8(program, sizeof(program)) <= 0)
		program[0] = '\0';
	slash = strrchr(program, '\\');
	if (slash)
		strcpy(slash + 1, "outguess.exe");
	basename_from_path(input, filename, sizeof(filename));
	pathLength = snprintf(output, sizeof(output), "%s\\%s", outputDirectory, filename);
	if (pathLength < 0 || pathLength >= (int)sizeof(output)) {
		fprintf(stderr, og_L("[fail] output path too long: %s\n", "[失败] 输出路径过长: %s\n"), input);
		return 1;
	}
	if (strcmp(mode, "extract") == 0) {
		char *extension = strrchr(output, '.');
		if (extension)
			strcpy(extension, ".txt");
		else
			strcat(output, ".txt");
	}
	if (message && strcmp(input, message) == 0) {
		fprintf(stderr, og_L("[fail] message cannot be the image: %s\n", "[失败] 消息不能是图片本身: %s\n"), input);
		return 1;
	}
	if (_stricmp(input, output) == 0) {
		fprintf(stderr, og_L("[fail] output cannot be the same image: %s\n", "[失败] 输出不能与输入图片相同: %s\n"), input);
		return 1;
	}
	if (!force && path_exists_utf8(output)) {
		printf(og_L("[skipped] output already exists: %s\n", "[已跳过] 输出已存在: %s\n"), output);
		return 0;
	}
	quote(inputQuoted, sizeof(inputQuoted), input);
	quote(outputQuoted, sizeof(outputQuoted), output);
	if (key)
		quote(keyQuoted, sizeof(keyQuoted), key);
	if (key)
		snprintf(keyArgument, sizeof(keyArgument), "-k %s ", keyQuoted);
	else
		keyArgument[0] = '\0';
	if (strcmp(mode, "embed") == 0) {
		quote(messageQuoted, sizeof(messageQuoted), message);
		if (quality) {
			snprintf(command, sizeof(command), "\"%s\" %s%s-p %s -d %s %s %s",
				program, keyArgument, errorCorrection ? "-e " : "", quality,
				messageQuoted, inputQuoted, outputQuoted);
		} else {
			snprintf(command, sizeof(command), "\"%s\" %s%s-d %s %s %s",
				program, keyArgument, errorCorrection ? "-e " : "",
				messageQuoted, inputQuoted, outputQuoted);
		}
	} else {
		snprintf(command, sizeof(command), "\"%s\" %s%s-r %s %s",
			program, keyArgument, errorCorrection ? "-e " : "", inputQuoted,
			outputQuoted);
	}
	printf(og_L("[%s] %s\nCommand: %s\n", "[%s] %s\n命令: %s\n"), mode, input, command);
	int exitCode = run_process_utf8(command);
	if (exitCode < 0) {
		fprintf(stderr, og_L("Could not start outguess.exe.\n", "无法启动 outguess.exe。\n"));
		return 1;
	}
	return exitCode;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
	/* 让命令行参数（如中文图片路径、输出目录、-k 密钥）按 UTF-8 解析，
	 * 而非被系统 ANSI 代码页误解码 */
	win32_to_utf8_args(&argc, &argv);
#endif
	og_i18n_init(&argc, &argv);

	WIN32_FIND_DATAW wdata;
	HANDLE search;
	char *key = NULL, *message = NULL, *pattern = NULL, *output = NULL, *quality = NULL;
	char directory[MAX_PATH], searchPattern[MAX_PATH], input[MAX_PATH];
	char *slash;
	int processed = 0, failed = 0;
	int errorCorrection = has_option(argc, argv, "-e");
	int force = has_option(argc, argv, "-f");
	if (argc < 2 || (strcmp(argv[1], "embed") != 0 && strcmp(argv[1], "extract") != 0) ||
		!get_option(argc, argv, "-i", &pattern) ||
		!get_option(argc, argv, "-o", &output) ||
		(has_option(argc, argv, "-p") && !get_option(argc, argv, "-p", &quality)) ||
		(strcmp(argv[1], "embed") == 0 && !get_option(argc, argv, "-m", &message))) {
		usage(argv[0]);
		return 2;
	}
	if (quality) {
		char *end;
		long value;
		errno = 0;
		value = strtol(quality, &end, 10);
		if (errno || *quality == '\0' || *end != '\0' || value < 75 || value > 100) {
			fprintf(stderr, og_L("JPEG quality must be between 75 and 100.\n", "JPEG 质量必须在 75 到 100 之间。\n"));
			return 2;
		}
	}
	if (message && !path_exists_utf8(message)) {
		fprintf(stderr, og_L("Message not found: %s\n", "未找到消息文件: %s\n"), message);
		return 1;
	}
	if (!ensure_directory(output)) {
		fprintf(stderr, og_L("Could not create output directory: %s\n", "无法创建输出目录: %s\n"), output);
		return 1;
	}
	strncpy(searchPattern, pattern, sizeof(searchPattern) - 1);
	searchPattern[sizeof(searchPattern) - 1] = '\0';
	slash = strrchr(searchPattern, '\\');
	if (!slash)
		slash = strrchr(searchPattern, '/');
	if (slash) {
		*slash = '\0';
		strncpy(directory, searchPattern, sizeof(directory) - 1);
		directory[sizeof(directory) - 1] = '\0';
		strcpy(searchPattern, slash + 1);
	} else {
		strcpy(directory, ".");
	}
	if (snprintf(input, sizeof(input), "%s\\%s", directory, searchPattern) >= (int)sizeof(input)) {
		fprintf(stderr, og_L("Input pattern too long: %s\n", "输入通配符过长: %s\n"), pattern);
		return 2;
	}
	search = find_first_utf8(input, &wdata);
	if (search == INVALID_HANDLE_VALUE) {
		fprintf(stderr, og_L("No files found for: %s\n", "未找到匹配文件: %s\n"), pattern);
		return 1;
	}
	do {
		if (!(wdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			char fname[MAX_PATH];
			if (find_name_utf8(&wdata, fname, sizeof(fname)) < 0) {
				printf(og_L("[skipped] filename too long.\n", "[已跳过] 文件名过长。\n"));
				continue;
			}
			if (snprintf(input, sizeof(input), "%s\\%s", directory, fname) >= (int)sizeof(input)) {
				fprintf(stderr, og_L("[skipped] path too long: %s\n", "[已跳过] 路径过长: %s\n"), fname);
				continue;
			}
			processed++;
			const char *extension = strrchr(input, '.');
			if (!extension || (_stricmp(extension, ".jpg") != 0 && _stricmp(extension, ".jpeg") != 0)) {
				printf(og_L("[skipped] not a JPEG: %s\n", "[已跳过] 不是 JPEG: %s\n"), input);
				continue;
			}
			if (process_file(argv[1], key, message, input, output,
				errorCorrection, quality, force) != 0)
				failed++;
		}
	} while (find_next_utf8(search, &wdata));
	FindClose(search);
	printf(og_L("Processed: %d | Succeeded: %d | Failed: %d\n",
		"已处理: %d | 成功: %d | 失败: %d\n"),
		processed, processed - failed, failed);
	return failed ? 1 : 0;
}
