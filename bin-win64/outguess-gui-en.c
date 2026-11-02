/*
 * outguess-gui-en.c - Native Win32 GUI for OutGuess (English edition).
 *
 * This is the English-language counterpart of outguess-gui.c. The two files
 * share identical logic; only the user-visible strings and comments differ.
 * outguess-gui.c shows a Simplified Chinese interface; this file shows an
 * English interface. Every string here is plain ASCII so the program renders
 * correctly on any Windows locale with zero font / code-page risk, while the
 * Unicode (W) APIs and UTF-8 subprocess handling are kept so that Chinese or
 * otherwise non-ASCII file paths and keys still work on every language build.
 *
 * The GUI does not re-implement any steganography: it merely assembles a
 * command line and invokes outguess.exe (which must live in the same folder),
 * captures its output, and shows it in the on-screen log.
 *
 * Built with MinGW-w64, statically linked, with no dependencies other than
 * the DLLs every Windows ships (comctl32, comdlg32).
 *
 * This program uses the Unicode (W) flavored Win32 APIs together with L""
 * wide string literals, so the interface (and any other language's text)
 * displays correctly on every language edition of Windows, independent of the
 * system ANSI code page (e.g. GBK on Chinese Windows). Only the part that
 * spawns background processes such as outguess.exe still uses the A-flavored
 * APIs with ASCII 8.3 (short) paths, so the child processes can open files
 * correctly under any code page.
 */
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

/* ---- Control IDs ---- */
#define ID_RADIO_EMBED   100
#define ID_RADIO_EXTRACT 101
#define ID_EDIT_IMGIN    110
#define ID_BTN_IMGIN     111
#define ID_EDIT_MSGFILE  120
#define ID_BTN_MSGFILE   121
#define ID_LBL_MSGFILE   122
#define ID_EDIT_IMGOUT   130
#define ID_BTN_IMGOUT    131
#define ID_LBL_IMGOUT    132
#define ID_EDIT_KEY      140
#define ID_CHK_ECC       150
#define ID_CHK_SHOWKEY   151
#define ID_BTN_RUN       160
#define ID_EDIT_LOG      170
#define ID_BTN_CLEAR     171
#define ID_BTN_OPEN      172
#define ID_BTN_CAPACITY  173
#define ID_BTN_COPY_LOG  174
#define ID_CHK_NO_OVERWRITE 152
#define ID_EDIT_QUALITY  180
#define ID_EDIT_ITER     181
#define ID_LBL_QUALITY   182
#define ID_LBL_ITER      183
#define ID_STATUS        184
#define ID_PROGRESS      185
#define WM_RUN_COMPLETE  (WM_APP + 1)
#define WM_LOG_APPEND    (WM_APP + 2)

static HWND hMain, hImgIn, hMsgFile, hLblMsgFile, hImgOut, hLblImgOut,
			hBtnImgIn, hBtnMsgFile, hBtnImgOut, hKey, hEcc, hShowKey,
			hLog, hRadioEmbed, hRadioExtract, hLblQuality, hLblIter,
			hQuality, hIter, hStatus, hProgress, hBtnOpen, hBtnCapacity,
			hBtnCopyLog, hNoOverwrite;
static HFONT hFont;
static unsigned long long lastPayloadBytes;
static int operationRunning;

static void AddHistory(HWND control, const wchar_t *value)
{
	if (!value[0])
		return;
	if (SendMessageW(control, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)value) == CB_ERR)
		SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)value);
}

static void CopyLogToClipboard(void)
{
	int length = GetWindowTextLengthW(hLog);
	HGLOBAL memory;
	wchar_t *text;
	if (length <= 0 || !OpenClipboard(hMain))
		return;
	memory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(length + 1) * sizeof(wchar_t));
	if (!memory) {
		CloseClipboard();
		return;
	}
	text = (wchar_t *)GlobalLock(memory);
	GetWindowTextW(hLog, text, length + 1);
	GlobalUnlock(memory);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, memory);
	CloseClipboard();
}

static void RememberCurrentPaths(void)
{
	wchar_t value[MAX_PATH];
	GetWindowTextW(hImgIn, value, sizeof(value) / sizeof(value[0]));
	AddHistory(hImgIn, value);
	GetWindowTextW(hMsgFile, value, sizeof(value) / sizeof(value[0]));
	AddHistory(hMsgFile, value);
	GetWindowTextW(hImgOut, value, sizeof(value) / sizeof(value[0]));
	AddHistory(hImgOut, value);
}

static void QueueLog(const wchar_t *text)
{
	size_t length = wcslen(text);
	wchar_t *copy = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
	if (!copy)
		return;
	wcscpy(copy, text);
	if (!PostMessageW(hMain, WM_LOG_APPEND, 0, (LPARAM)copy))
		free(copy);
}

static void QueueLogLine(const wchar_t *text)
{
	wchar_t line[4096];
	_snwprintf(line, sizeof(line) / sizeof(line[0]), L"%s\r\n", text);
	QueueLog(line);
}

/* Append a byte string in the system default code page (CP_ACP) coming from
 * the outside world (outguess.exe output, paths, ...) to the log. */
static void QueueLogAcp(const char *text)
{
	int wlen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	if (wlen <= 0)
		return;
	wchar_t *w = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
	if (!w)
		return;
	MultiByteToWideChar(CP_ACP, 0, text, -1, w, wlen);
	QueueLog(w);
	free(w);
}

static void AppendLog(const wchar_t *txt)
{
	int len = GetWindowTextLengthW(hLog);
	SendMessageW(hLog, EM_SETSEL, len, len);
	SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)txt);
	SendMessageW(hLog, EM_SCROLLCARET, 0, 0);
}

static void AppendLogLine(const wchar_t *txt)
{
	AppendLog(txt);
	AppendLog(L"\r\n");
}

/* Append a CP_ACP byte string directly to the log (for use inside the UI
 * thread). */
static void AppendLogAcp(const char *text)
{
	int wlen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	if (wlen <= 0)
		return;
	wchar_t *w = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
	if (!w)
		return;
	MultiByteToWideChar(CP_ACP, 0, text, -1, w, wlen);
	AppendLog(w);
	free(w);
}

typedef struct {
	char args[4096];
	char tempMessageFile[MAX_PATH];
	int embedding;
} RunContext;

/* Current mode: 1 = embed, 0 = extract */
static int ModoEmbutir(void)
{
	return SendMessageW(hRadioEmbed, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void AppendLogReady(void)
{
	AppendLogLine(L"OutGuess GUI is ready. Choose a mode, fill in the fields, and click Run.");
}

/* Build the outguess.exe path: same folder as this program. */
static void GetOutguessPath(char *buf, DWORD bufsize)
{
	char self[MAX_PATH];
	GetModuleFileNameA(NULL, self, MAX_PATH);
	char *slash = strrchr(self, '\\');
	if (slash)
		*(slash + 1) = '\0';
	else
		self[0] = '\0';
	snprintf(buf, bufsize, "%soutguess.exe", self);
}

static int BrowseOpen(HWND owner, wchar_t *buf, DWORD bufsize, const wchar_t *filter, const wchar_t *title)
{
	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = owner;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufsize;
	ofn.lpstrFilter = filter;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	return GetOpenFileNameW(&ofn);
}

static int BrowseSave(HWND owner, wchar_t *buf, DWORD bufsize, const wchar_t *filter, const wchar_t *title, const wchar_t *defExt)
{
	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = owner;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufsize;
	ofn.lpstrFilter = filter;
	ofn.lpstrTitle = title;
	ofn.lpstrDefExt = defExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	return GetSaveFileNameW(&ofn);
}

/* Run outguess.exe with the given arguments, capture stdout+stderr and write
 * them to the log. Returns the exit code (or -1 if it could not start). */
static int RunOutguess(const char *args)
{
	char exePath[MAX_PATH];
	GetOutguessPath(exePath, MAX_PATH);

	if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
		QueueLogLine(L"outguess.exe not found at:");
		QueueLogAcp(exePath);
		QueueLogLine(L"Please place outguess-gui.exe and outguess.exe in the same folder.");
		return -1;
	}

	/* Convert the exe path (system default code page) and the arguments
	 * (UTF-8, which may contain a non-ASCII key) to UTF-16 and launch via
	 * CreateProcessW. This avoids the key being mis-decoded under a non-Chinese
	 * Windows (where the ANSI code page differs), so the arguments are passed
	 * correctly on every language edition of Windows. */
	wchar_t exePathW[MAX_PATH];
	if (MultiByteToWideChar(CP_ACP, 0, exePath, -1, exePathW, MAX_PATH) == 0) {
		QueueLogLine(L"[Error] Unable to resolve the outguess.exe path");
		return -1;
	}
	int argsWlen = MultiByteToWideChar(CP_UTF8, 0, args, -1, NULL, 0);
	if (argsWlen <= 0) {
		QueueLogLine(L"[Error] Unable to parse command-line arguments");
		return -1;
	}
	wchar_t *argsW = (wchar_t *)malloc((size_t)argsWlen * sizeof(wchar_t));
	if (!argsW) {
		QueueLogLine(L"[Error] Out of memory");
		return -1;
	}
	MultiByteToWideChar(CP_UTF8, 0, args, -1, argsW, argsWlen);

	wchar_t cmdlineW[4096 + MAX_PATH];
	_snwprintf(cmdlineW, sizeof(cmdlineW) / sizeof(wchar_t), L"\"%s\" %s",
		exePathW, argsW);
	free(argsW);

	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE hReadPipe, hWritePipe;
	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		QueueLogLine(L"[Error] Failed to create output pipe");
		return -1;
	}
	SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.hStdInput = NULL;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	QueueLogLine(L"---");
	QueueLogLine(cmdlineW);

	BOOL ok = CreateProcessW(NULL, cmdlineW, NULL, NULL, TRUE,
	                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	CloseHandle(hWritePipe);

	if (!ok) {
		CloseHandle(hReadPipe);
		QueueLogLine(L"[Error] Unable to start outguess.exe");
		return -1;
	}

	char buf[4096];
	DWORD n;
	while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
		buf[n] = '\0';
		QueueLogAcp(buf);
	}
	CloseHandle(hReadPipe);

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (int)exitCode;
}

static void QuoteInto(char *dst, size_t dstsize, const char *src)
{
	snprintf(dst, dstsize, "\"%s\"", src);
}

static void GetProcessPath(HWND control, char *path, size_t pathsize)
{
	wchar_t widePath[MAX_PATH];
	wchar_t shortPath[MAX_PATH];
	GetWindowTextW(control, widePath, sizeof(widePath) / sizeof(widePath[0]));
	DWORD shortLength = GetShortPathNameW(widePath, shortPath,
		sizeof(shortPath) / sizeof(shortPath[0]));
	const wchar_t *source = shortLength > 0 && shortLength < sizeof(shortPath) / sizeof(shortPath[0])
		? shortPath : widePath;
	int converted = WideCharToMultiByte(CP_ACP, 0, source, -1, path,
		(int)pathsize, NULL, NULL);
	if (converted == 0)
		path[0] = '\0';
}

static int GetUtf8ControlText(HWND control, char *text, size_t textsize)
{
	wchar_t wideText[1024];
	GetWindowTextW(control, wideText, sizeof(wideText) / sizeof(wideText[0]));
	return WideCharToMultiByte(CP_UTF8, 0, wideText, -1, text,
		(int)textsize, NULL, NULL) != 0;
}

static int ValidateOutputPath(const char *input, const char *output)
{
	char parent[MAX_PATH];
	DWORD attributes;
	if (_stricmp(input, output) == 0) {
		MessageBoxW(hMain, L"The output file must differ from the input file.",
			L"Invalid target", MB_ICONWARNING);
		return 0;
	}
	attributes = GetFileAttributesA(output);
	if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		MessageBoxW(hMain, L"The specified target is a folder, not a file.",
			L"Invalid target", MB_ICONWARNING);
		return 0;
	}
	strncpy(parent, output, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';
	char *slash = strrchr(parent, '\\');
	if (!slash)
		slash = strrchr(parent, '/');
	if (slash)
		*slash = '\0';
	else
		strcpy(parent, ".");
	attributes = GetFileAttributesA(parent);
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		MessageBoxW(hMain, L"The destination folder does not exist.", L"Invalid target", MB_ICONWARNING);
		return 0;
	}
	attributes = GetFileAttributesA(output);
	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if (MessageBoxW(hMain, L"The target file already exists. Replace it?",
			L"Confirm replacement", MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
	}
	return 1;
}

static int CreateTempMessageFile(const wchar_t *message, char *path, DWORD pathsize)
{
	char tempDir[MAX_PATH];
	char *utf8Message;
	DWORD tempDirLength = GetTempPathA(sizeof(tempDir), tempDir);
	if (tempDirLength == 0 || tempDirLength >= sizeof(tempDir))
		return 0;
	if (GetTempFileNameA(tempDir, "ogs", 0, path) == 0)
		return 0;

	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
	if (utf8Length <= 0) {
		DeleteFileA(path);
		return 0;
	}
	utf8Message = (char *)malloc((size_t)utf8Length);
	if (!utf8Message || WideCharToMultiByte(CP_UTF8, 0, message, -1,
	    utf8Message, utf8Length, NULL, NULL) == 0) {
		free(utf8Message);
		DeleteFileA(path);
		return 0;
	}

	FILE *file = fopen(path, "wb");
	if (!file) {
		free(utf8Message);
		DeleteFileA(path);
		return 0;
	}

	size_t messageLength = (size_t)utf8Length - 1;
	int written = fwrite(utf8Message, 1, messageLength, file) == messageLength;
	free(utf8Message);
	if (fclose(file) != 0)
		written = 0;
	if (!written) {
		DeleteFileA(path);
		return 0;
	}
	return 1;
}

static DWORD WINAPI RunThread(LPVOID parameter)
{
	RunContext *context = (RunContext *)parameter;
	int result = RunOutguess(context->args);
	if (context->tempMessageFile[0] != '\0')
		DeleteFileA(context->tempMessageFile);
	PostMessageW(hMain, WM_RUN_COMPLETE, (WPARAM)result, (LPARAM)context->embedding);
	free(context);
	return 0;
}

static void OpenContainingFolder(const wchar_t *path)
{
	wchar_t folder[MAX_PATH];
	wcsncpy(folder, path, sizeof(folder) / sizeof(folder[0]) - 1);
	folder[sizeof(folder) / sizeof(folder[0]) - 1] = L'\0';
	wchar_t *slash = wcsrchr(folder, L'\\');
	if (!slash)
		slash = wcsrchr(folder, L'/');
	if (slash)
		*slash = L'\0';
	else
		wcscpy(folder, L".");
	ShellExecuteW(hMain, L"open", folder, NULL, NULL, SW_SHOWNORMAL);
}

static void RunCapacity(void)
{
	char image[MAX_PATH], quality[16], tool[MAX_PATH], command[2048];
	GetProcessPath(hImgIn, image, sizeof(image));
	GetWindowTextA(hQuality, quality, sizeof(quality));
	if (!image[0] || GetFileAttributesA(image) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxW(hMain, L"Please select a valid JPEG image first.", L"Capacity", MB_ICONWARNING);
		return;
	}
	GetOutguessPath(tool, sizeof(tool));
	char *slash = strrchr(tool, '\\');
	if (slash)
		strcpy(slash + 1, "capacity.exe");
	char quotedImage[MAX_PATH + 2];
	QuoteInto(quotedImage, sizeof(quotedImage), image);
	snprintf(command, sizeof(command), "-e -p %s %s", quality, quotedImage);
	char fullCommand[4096];
	snprintf(fullCommand, sizeof(fullCommand), "\"%s\" %s", tool, command);
	AppendLogLine(L"--- Capacity ---");
	AppendLogAcp(fullCommand);
	SECURITY_ATTRIBUTES security;
	HANDLE readPipe, writePipe;
	ZeroMemory(&security, sizeof(security));
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;

	/* Convert the exe path (system default code page CP_ACP) and the arguments
	 * (UTF-8) to UTF-16 separately, then launch with CreateProcessW so that
	 * non-ASCII paths / arguments are passed correctly on every language
	 * edition of Windows (same strategy as RunOutguess). */
	wchar_t toolW[MAX_PATH];
	if (MultiByteToWideChar(CP_ACP, 0, tool, -1, toolW, MAX_PATH) == 0) {
		AppendLogLine(L"[Error] Unable to resolve the capacity.exe path");
		return;
	}
	int cmdWlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
	if (cmdWlen <= 0) {
		AppendLogLine(L"[Error] Unable to parse command-line arguments");
		return;
	}
	wchar_t *cmdW = (wchar_t *)malloc((size_t)cmdWlen * sizeof(wchar_t));
	if (!cmdW) {
		AppendLogLine(L"[Error] Out of memory");
		return;
	}
	MultiByteToWideChar(CP_UTF8, 0, command, -1, cmdW, cmdWlen);

	wchar_t cmdlineW[4096 + MAX_PATH];
	_snwprintf(cmdlineW, sizeof(cmdlineW) / sizeof(wchar_t), L"\"%s\" %s",
		toolW, cmdW);
	free(cmdW);

	if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
		AppendLogLine(L"[Error] Unable to get capacity.");
		return;
	}
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	startup.hStdOutput = writePipe;
	startup.hStdError = writePipe;

	PROCESS_INFORMATION process;
	ZeroMemory(&process, sizeof(process));

	AppendLogLine(cmdlineW);

	if (!CreateProcessW(NULL, cmdlineW, NULL, NULL, TRUE, CREATE_NO_WINDOW,
		NULL, NULL, &startup, &process)) {
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		AppendLogLine(L"[Error] capacity.exe not found.");
		return;
	}
	CloseHandle(writePipe);
	char output[4096];
	DWORD bytesRead;
	while (ReadFile(readPipe, output, sizeof(output) - 1, &bytesRead, NULL) && bytesRead > 0) {
		output[bytesRead] = '\0';
		AppendLogAcp(output);
	}
	CloseHandle(readPipe);
	WaitForSingleObject(process.hProcess, INFINITE);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
}

static void OnRun(void)
{
	char imgin[MAX_PATH], msgfile[MAX_PATH], imgout[MAX_PATH], key[2048];
	char quality[16], iterations[16];
	wchar_t messageText[2048];
	GetProcessPath(hImgIn, imgin, sizeof(imgin));
	GetProcessPath(hMsgFile, msgfile, sizeof(msgfile));
	GetWindowTextW(hMsgFile, messageText, sizeof(messageText) / sizeof(messageText[0]));
	GetProcessPath(hImgOut, imgout, sizeof(imgout));
	if (!GetUtf8ControlText(hKey, key, sizeof(key))) {
		MessageBoxW(hMain, L"Unable to read the entered key.", L"Error", MB_ICONERROR);
		return;
	}
	GetWindowTextA(hQuality, quality, sizeof(quality));
	GetWindowTextA(hIter, iterations, sizeof(iterations));

	if (imgin[0] == '\0') {
		MessageBoxW(hMain, L"Please select an input JPEG image.", L"Missing field", MB_ICONWARNING);
		return;
	}
	if (GetFileAttributesA(imgin) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxW(hMain, L"Input image not found.", L"Invalid file", MB_ICONWARNING);
		return;
	}
	int jpegQuality = atoi(quality);
	int iterationLimit = atoi(iterations);
	if (ModoEmbutir() && (jpegQuality < 75 || jpegQuality > 100)) {
		MessageBoxW(hMain, L"JPEG quality must be between 75 and 100.", L"Invalid value", MB_ICONWARNING);
		return;
	}
	if (ModoEmbutir() && (iterationLimit < 0 || iterationLimit > 65535)) {
		MessageBoxW(hMain, L"Iteration limit must be between 0 and 65535.", L"Invalid value", MB_ICONWARNING);
		return;
	}
	int ecc = SendMessageW(hEcc, BM_GETCHECK, 0, 0) == BST_CHECKED;
	char q_imgin[MAX_PATH + 2], q_msgfile[MAX_PATH + 2], q_imgout[MAX_PATH + 2], q_key[2050], keyArg[2070];
	QuoteInto(q_imgin, sizeof(q_imgin), imgin);
	if (key[0] != '\0') {
		QuoteInto(q_key, sizeof(q_key), key);
		snprintf(keyArg, sizeof(keyArg), "-k %s ", q_key);
	} else {
		strcpy(keyArg, "");
	}

	RunContext *context = (RunContext *)calloc(1, sizeof(RunContext));
	if (!context) {
		MessageBoxW(hMain, L"Unable to start the operation.", L"Error", MB_ICONERROR);
		return;
	}
	context->embedding = ModoEmbutir();

	if (ModoEmbutir()) {
		if (msgfile[0] == '\0') {
			MessageBoxW(hMain, L"Please enter a message or select a file.", L"Missing field", MB_ICONWARNING);
			free(context);
			return;
		}
		if (imgout[0] == '\0') {
			MessageBoxW(hMain, L"Please choose where to save the output image.", L"Missing field", MB_ICONWARNING);
			free(context);
			return;
		}
		if (SendMessageW(hNoOverwrite, BM_GETCHECK, 0, 0) == BST_CHECKED &&
			GetFileAttributesA(imgout) != INVALID_FILE_ATTRIBUTES) {
			MessageBoxW(hMain, L"Output file already exists and 'Do not overwrite' is enabled.",
				L"Target protected", MB_ICONWARNING);
			free(context);
			return;
		}
		if (!ValidateOutputPath(imgin, imgout)) {
			free(context);
			return;
		}
		if (MessageBoxW(hMain, L"Start hiding the message into the image?",
			L"Confirm operation", MB_YESNO | MB_ICONQUESTION) != IDYES) {
			free(context);
			return;
		}
		RememberCurrentPaths();
		DWORD messageAttributes = GetFileAttributesA(msgfile);
		if (messageAttributes == INVALID_FILE_ATTRIBUTES ||
		    (messageAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (!CreateTempMessageFile(messageText, context->tempMessageFile,
			    sizeof(context->tempMessageFile))) {
				MessageBoxW(hMain, L"Unable to prepare the message.", L"Error", MB_ICONERROR);
				free(context);
				return;
			}
			QuoteInto(q_msgfile, sizeof(q_msgfile), context->tempMessageFile);
		} else {
			QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		}
		QuoteInto(q_imgout, sizeof(q_imgout), imgout);
		snprintf(context->args, sizeof(context->args), "%s%s-p %s -d %s %s %s",
			keyArg, ecc ? "-e " : "", quality, q_msgfile, q_imgin, q_imgout);
		if (iterations[0]) {
			char suffix[64];
			snprintf(suffix, sizeof(suffix), " -i %s", iterations);
			strncat(context->args, suffix, sizeof(context->args) - strlen(context->args) - 1);
		}
	} else {
		if (msgfile[0] == '\0') {
			MessageBoxW(hMain, L"Please choose where to save the extracted message.", L"Missing field", MB_ICONWARNING);
			free(context);
			return;
		}
		if (SendMessageW(hNoOverwrite, BM_GETCHECK, 0, 0) == BST_CHECKED &&
			GetFileAttributesA(msgfile) != INVALID_FILE_ATTRIBUTES) {
			MessageBoxW(hMain, L"Extracted file already exists and 'Do not overwrite' is enabled.",
				L"Target protected", MB_ICONWARNING);
			free(context);
			return;
		}
		if (!ValidateOutputPath(imgin, msgfile)) {
			free(context);
			return;
		}
		if (MessageBoxW(hMain, L"Start extracting the message?",
			L"Confirm operation", MB_YESNO | MB_ICONQUESTION) != IDYES) {
			free(context);
			return;
		}
		RememberCurrentPaths();
		QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		snprintf(context->args, sizeof(context->args), "%s%s-r %s %s",
			keyArg, ecc ? "-e " : "", q_imgin, q_msgfile);
	}

	lastPayloadBytes = 0;
	if (context->embedding) {
		DWORD attributes = GetFileAttributesA(msgfile);
		if (attributes != INVALID_FILE_ATTRIBUTES &&
		    !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			HANDLE file = CreateFileA(msgfile, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			LARGE_INTEGER size;
			if (file != INVALID_HANDLE_VALUE && GetFileSizeEx(file, &size))
				lastPayloadBytes = (unsigned long long)size.QuadPart;
			if (file != INVALID_HANDLE_VALUE)
				CloseHandle(file);
		} else {
			int utf8Length = WideCharToMultiByte(CP_UTF8, 0, messageText, -1,
				NULL, 0, NULL, NULL);
			if (utf8Length > 0)
				lastPayloadBytes = (unsigned long long)utf8Length - 1;
		}
	}
	wchar_t status[128];
	if (context->embedding)
		swprintf(status, 128, L"Payload: %llu bytes | see log for capacity", lastPayloadBytes);
	else
		wcscpy(status, L"Preparing extraction...");
	SetWindowTextW(hStatus, status);

	EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), FALSE);
	EnableWindow(hBtnOpen, FALSE);
	operationRunning = 1;
	SendMessageW(hProgress, PBM_SETMARQUEE, TRUE, 30);
	SetWindowTextW(hStatus, context->embedding ? L"Hiding message..." : L"Extracting message...");
	HANDLE thread = CreateThread(NULL, 0, RunThread, context, 0, NULL);
	if (!thread) {
		if (context->tempMessageFile[0] != '\0')
			DeleteFileA(context->tempMessageFile);
		free(context);
		SendMessageW(hProgress, PBM_SETMARQUEE, FALSE, 0);
		EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), TRUE);
		EnableWindow(hBtnOpen, TRUE);
		operationRunning = 0;
		SetWindowTextW(hStatus, L"Ready");
		MessageBoxW(hMain, L"Unable to start the operation.", L"Error", MB_ICONERROR);
		return;
	}
	CloseHandle(thread);
}

static void UpdateModeVisibility(void)
{
	int embutir = ModoEmbutir();
	SetWindowTextW(hLblMsgFile, embutir ? L"Message or file to hide:" : L"Save extracted message to:");
	ShowWindow(hLblImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hBtnImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hLblQuality, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hQuality, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hLblIter, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hIter, embutir ? SW_SHOW : SW_HIDE);
	EnableWindow(hBtnOpen, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_CREATE: {
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

		#define MKSTATIC(txt, x, y, w, h) do { \
			HWND h_ = CreateWindowW(L"STATIC", txt, WS_CHILD | WS_VISIBLE, \
				x, y, w, h, hwnd, NULL, NULL, NULL); \
			SendMessageW(h_, WM_SETFONT, (WPARAM)hFont, TRUE); \
		} while (0)

		MKSTATIC(L"Mode:", 12, 14, 60, 20);
		hRadioEmbed = CreateWindowW(L"BUTTON", L"Hide message",
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
			80, 12, 160, 20, hwnd, (HMENU)ID_RADIO_EMBED, NULL, NULL);
		hRadioExtract = CreateWindowW(L"BUTTON", L"Extract message",
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
			250, 12, 160, 20, hwnd, (HMENU)ID_RADIO_EXTRACT, NULL, NULL);
		SendMessageW(hRadioEmbed, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hRadioExtract, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hRadioEmbed, BM_SETCHECK, BST_CHECKED, 0);

		MKSTATIC(L"Input JPEG image:", 12, 50, 200, 20);
		hImgIn = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 72, 470, 24, hwnd, (HMENU)ID_EDIT_IMGIN, NULL, NULL);
		hBtnImgIn = CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE,
			492, 72, 100, 24, hwnd, (HMENU)ID_BTN_IMGIN, NULL, NULL);
		SendMessageW(hImgIn, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblMsgFile = CreateWindowW(L"STATIC", L"Message or file to hide:", WS_CHILD | WS_VISIBLE,
			12, 108, 400, 20, hwnd, (HMENU)ID_LBL_MSGFILE, NULL, NULL);
		hMsgFile = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 130, 470, 24, hwnd, (HMENU)ID_EDIT_MSGFILE, NULL, NULL);
		hBtnMsgFile = CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE,
			492, 130, 100, 24, hwnd, (HMENU)ID_BTN_MSGFILE, NULL, NULL);
		SendMessageW(hLblMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblImgOut = CreateWindowW(L"STATIC", L"Output image (with message):", WS_CHILD | WS_VISIBLE,
			12, 166, 300, 20, hwnd, (HMENU)ID_LBL_IMGOUT, NULL, NULL);
		hImgOut = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 188, 470, 24, hwnd, (HMENU)ID_EDIT_IMGOUT, NULL, NULL);
		hBtnImgOut = CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE,
			492, 188, 100, 24, hwnd, (HMENU)ID_BTN_IMGOUT, NULL, NULL);
		SendMessageW(hLblImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblQuality = CreateWindowW(L"STATIC", L"JPEG quality (75-100):", WS_CHILD | WS_VISIBLE,
			12, 224, 150, 20, hwnd, (HMENU)ID_LBL_QUALITY, NULL, NULL);
		hQuality = CreateWindowW(L"EDIT", L"75", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
			165, 222, 60, 24, hwnd, (HMENU)ID_EDIT_QUALITY, NULL, NULL);
		hLblIter = CreateWindowW(L"STATIC", L"Iteration limit:", WS_CHILD | WS_VISIBLE,
			245, 224, 130, 20, hwnd, (HMENU)ID_LBL_ITER, NULL, NULL);
		hIter = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
			378, 222, 75, 24, hwnd, (HMENU)ID_EDIT_ITER, NULL, NULL);

		MKSTATIC(L"Key (optional):", 12, 260, 180, 20);
		hKey = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
			12, 282, 300, 24, hwnd, (HMENU)ID_EDIT_KEY, NULL, NULL);
		hShowKey = CreateWindowW(L"BUTTON", L"Show", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			322, 284, 90, 20, hwnd, (HMENU)ID_CHK_SHOWKEY, NULL, NULL);
		SendMessageW(hKey, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hShowKey, WM_SETFONT, (WPARAM)hFont, TRUE);

		hEcc = CreateWindowW(L"BUTTON", L"Use error correction (more robust)",
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12, 322, 320, 20, hwnd, (HMENU)ID_CHK_ECC, NULL, NULL);
		SendMessageW(hEcc, WM_SETFONT, (WPARAM)hFont, TRUE);
		hNoOverwrite = CreateWindowW(L"BUTTON", L"Do not overwrite existing files",
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12, 346, 300, 20, hwnd, (HMENU)ID_CHK_NO_OVERWRITE, NULL, NULL);
		SendMessageW(hNoOverwrite, BM_SETCHECK, BST_CHECKED, 0);
		SendMessageW(hNoOverwrite, WM_SETFONT, (WPARAM)hFont, TRUE);

		CreateWindowW(L"BUTTON", L"Clear log", WS_CHILD | WS_VISIBLE,
			12, 376, 100, 32, hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL);
		hBtnOpen = CreateWindowW(L"BUTTON", L"Open folder", WS_CHILD | WS_VISIBLE,
			125, 376, 110, 32, hwnd, (HMENU)ID_BTN_OPEN, NULL, NULL);
		EnableWindow(hBtnOpen, FALSE);
		hBtnCapacity = CreateWindowW(L"BUTTON", L"Check capacity", WS_CHILD | WS_VISIBLE,
			245, 376, 160, 32, hwnd, (HMENU)ID_BTN_CAPACITY, NULL, NULL);
		hBtnCopyLog = CreateWindowW(L"BUTTON", L"Copy log", WS_CHILD | WS_VISIBLE,
			12, 414, 100, 28, hwnd, (HMENU)ID_BTN_COPY_LOG, NULL, NULL);
		SendMessageW(hBtnCapacity, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessageW(hBtnCopyLog, WM_SETFONT, (WPARAM)hFont, TRUE);
		CreateWindowW(L"BUTTON", L"Run", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			420, 376, 172, 32, hwnd, (HMENU)ID_BTN_RUN, NULL, NULL);
		SendMessageW(GetDlgItem(hwnd, ID_BTN_RUN), WM_SETFONT, (WPARAM)hFont, TRUE);

		hStatus = CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE,
			125, 418, 280, 20, hwnd, (HMENU)ID_STATUS, NULL, NULL);
		hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
			420, 414, 172, 16, hwnd, (HMENU)ID_PROGRESS, NULL, NULL);
		SendMessageW(hProgress, PBM_SETMARQUEE, FALSE, 0);

		MKSTATIC(L"Run log", 12, 454, 150, 20);
		hLog = CreateWindowW(L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
			ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			12, 478, 580, 180, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);
		SendMessageW(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

		AppendLogReady();
		UpdateModeVisibility();
		DragAcceptFiles(hwnd, TRUE);
		return 0;
	}

	case WM_COMMAND: {
		int id = LOWORD(wp);
		int code = HIWORD(wp);
		switch (id) {
		case ID_RADIO_EMBED:
		case ID_RADIO_EXTRACT:
			if (code == BN_CLICKED)
				UpdateModeVisibility();
			break;
		case ID_BTN_IMGIN: {
			wchar_t buf[MAX_PATH] = L"";
			if (BrowseOpen(hwnd, buf, MAX_PATH,
			    L"JPEG images\0*.jpg;*.jpeg\0All files\0*.*\0",
			    L"Select JPEG image"))
				SetWindowTextW(hImgIn, buf);
			break;
		}
		case ID_BTN_MSGFILE: {
			wchar_t buf[MAX_PATH] = L"";
			if (ModoEmbutir()) {
				if (BrowseOpen(hwnd, buf, MAX_PATH,
				    L"All files\0*.*\0", L"Select message file"))
					SetWindowTextW(hMsgFile, buf);
			} else {
				if (BrowseSave(hwnd, buf, MAX_PATH,
				    L"Text files\0*.txt\0All files\0*.*\0",
				    L"Save extracted message as", L"txt"))
					SetWindowTextW(hMsgFile, buf);
			}
			break;
		}
		case ID_BTN_IMGOUT: {
			wchar_t buf[MAX_PATH] = L"";
			if (BrowseSave(hwnd, buf, MAX_PATH,
			    L"JPEG images\0*.jpg\0All files\0*.*\0",
			    L"Save output image as", L"jpg"))
				SetWindowTextW(hImgOut, buf);
			break;
		}
		case ID_CHK_SHOWKEY: {
			int show = SendMessageW(hShowKey, BM_GETCHECK, 0, 0) == BST_CHECKED;
			SendMessageW(hKey, EM_SETPASSWORDCHAR, show ? 0 : (WPARAM)L'*', 0);
			InvalidateRect(hKey, NULL, TRUE);
			break;
		}
		case ID_BTN_RUN:
			OnRun();
			break;
		case ID_BTN_CLEAR:
			SetWindowTextW(hLog, L"");
			break;
		case ID_BTN_OPEN: {
			wchar_t path[MAX_PATH];
			GetWindowTextW(ModoEmbutir() ? hImgOut : hMsgFile, path, sizeof(path) / sizeof(path[0]));
			OpenContainingFolder(path);
			break;
		}
		case ID_BTN_CAPACITY:
			RunCapacity();
			break;
		case ID_BTN_COPY_LOG:
			CopyLogToClipboard();
			SetWindowTextW(hStatus, L"Log copied");
			break;
		}
		return 0;
	}

	case WM_LOG_APPEND:
		AppendLog((const wchar_t *)lp);
		free((void *)lp);
		return 0;

	case WM_RUN_COMPLETE: {
		int result = (int)wp;
		int embedding = (int)lp;
		EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), TRUE);
		operationRunning = 0;
		SendMessageW(hProgress, PBM_SETMARQUEE, FALSE, 0);
		if (result == 0) {
			EnableWindow(hBtnOpen, TRUE);
			wchar_t status[128];
			swprintf(status, 128, L"Done | payload: %llu bytes", lastPayloadBytes);
			SetWindowTextW(hStatus, status);
			AppendLogLine(embedding
				? L"[OK] Message hidden successfully."
				: L"[OK] Message extracted successfully.");
			MessageBoxW(hMain, embedding ? L"Message hidden successfully!" :
				L"Message extracted successfully!", L"OutGuess", MB_ICONINFORMATION);
		} else {
			SetWindowTextW(hStatus, L"Operation failed");
			AppendLogLine(L"[Failed] outguess.exe returned an error. See the log above.");
			MessageBoxW(hMain, L"Operation failed. See the log for details.",
				L"OutGuess", MB_ICONERROR);
		}
		return 0;
	}

	case WM_DROPFILES: {
		HDROP drop = (HDROP)wp;
		wchar_t path[MAX_PATH];
		if (DragQueryFileW(drop, 0, path, sizeof(path) / sizeof(path[0]))) {
			const wchar_t *extension = wcsrchr(path, L'.');
			int isJpeg = extension && (_wcsicmp(extension, L".jpg") == 0 ||
				_wcsicmp(extension, L".jpeg") == 0);
			if (isJpeg || !ModoEmbutir())
				SetWindowTextW(hImgIn, path);
			else
				SetWindowTextW(hMsgFile, path);
		}
		DragFinish(drop);
		return 0;
	}

	case WM_CLOSE:
		if (operationRunning) {
			MessageBoxW(hwnd, L"Please wait for the operation to finish before closing.",
				L"Operation in progress", MB_ICONINFORMATION);
			return 0;
		}
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int nShow)
{
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASSW wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"OutGuessGuiWndClass";
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
	RegisterClassW(&wc);

	hMain = CreateWindowW(L"OutGuessGuiWndClass", L"OutGuess - JPEG Steganography Tool",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 620, 704,
		NULL, NULL, hInst, NULL);

	ShowWindow(hMain, nShow);
	UpdateWindow(hMain);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}
