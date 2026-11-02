/*
 * i18n.c - 命令行界面（CLI）双语支持实现
 *
 * 默认简体中文；-L en / --lang en / --lang=en 切到英文。
 * 环境变量 OUTGUESS_LANG / LC_ALL / LANG 亦可设置（含 zh/CN 即中文，en 即英文）。
 */
#include "i18n.h"
#include <string.h>
#include <stdlib.h>

/* 默认简体中文，符合本地化目标；用 -L en 切到英文。 */
og_lang og_cur_lang = LANG_ZH;

static void apply_env(void)
{
	const char *e;

	if ((e = getenv("OUTGUESS_LANG")) != NULL && *e != '\0') {
		if (strncmp(e, "zh", 2) == 0 || strstr(e, "CN") != NULL)
			og_cur_lang = LANG_ZH;
		else if (strncmp(e, "en", 2) == 0)
			og_cur_lang = LANG_EN;
		return;
	}
	if ((e = getenv("LC_ALL")) != NULL && *e != '\0') {
		if (strstr(e, "zh") != NULL || strstr(e, "ZH") != NULL ||
		    strstr(e, "CN") != NULL)
			og_cur_lang = LANG_ZH;
		else if (strstr(e, "en") != NULL || strstr(e, "EN") != NULL)
			og_cur_lang = LANG_EN;
		return;
	}
	if ((e = getenv("LANG")) != NULL && *e != '\0') {
		if (strstr(e, "zh") != NULL || strstr(e, "ZH") != NULL ||
		    strstr(e, "CN") != NULL)
			og_cur_lang = LANG_ZH;
		else if (strstr(e, "en") != NULL || strstr(e, "EN") != NULL)
			og_cur_lang = LANG_EN;
	}
}

static void set_from_value(const char *v)
{
	if (strncmp(v, "zh", 2) == 0)
		og_cur_lang = LANG_ZH;
	else if (strncmp(v, "en", 2) == 0)
		og_cur_lang = LANG_EN;
}

void
og_i18n_init(int *pargc, char ***pargv)
{
	apply_env();

	char **argv = *pargv;
	int argc = *pargc;
	int w = 1;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-L") == 0 ||
		    strcmp(argv[i], "--lang") == 0) {
			if (i + 1 < argc) {
				set_from_value(argv[i + 1]);
				i++;	/* 跳过值 */
			}
			continue;
		}
		if (strncmp(argv[i], "--lang=", 7) == 0) {
			set_from_value(argv[i] + 7);
			continue;
		}
		argv[w++] = argv[i];
	}
	argv[w] = NULL;

	*pargc = w;
	*pargv = argv;
}
