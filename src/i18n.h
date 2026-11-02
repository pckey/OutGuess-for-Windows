/*
 * i18n.h - 命令行界面（CLI）双语支持
 *
 * 默认简体中文；用 -L en / --lang en（或 --lang=en）切换到英文。
 * 环境变量 OUTGUESS_LANG / LC_ALL / LANG 亦可设置：含 zh / CN 即中文，en 即英文。
 *
 * 用法（每个工具的 main 开头）：
 *     #ifdef _WIN32
 *         win32_to_utf8_args(&argc, &argv);
 *     #endif
 *     og_i18n_init(&argc, &argv);
 *
 * 字符串输出用 og_L(en, zh) 包裹，en/zh 必须含有完全一致的 printf 格式符：
 *     fprintf(stderr, og_L("Usage: %s\n", "用法: %s\n"), prog);
 */
#ifndef OUTGUESS_I18N_H
#define OUTGUESS_I18N_H

typedef enum { LANG_EN = 0, LANG_ZH = 1 } og_lang;

extern og_lang og_cur_lang;

/* 解析 -L/--lang（就地从 argv 移除该选项及其值，避免干扰后续参数解析），
 * 并读取环境变量。通过指针更新 argc/argv。 */
void og_i18n_init(int *pargc, char ***pargv);

/* 返回当前语言的格式串。en/zh 的 printf 格式符必须一致。 */
static inline const char *og_L(const char *en, const char *zh)
{
	return og_cur_lang == LANG_ZH ? zh : en;
}

#endif /* OUTGUESS_I18N_H */
