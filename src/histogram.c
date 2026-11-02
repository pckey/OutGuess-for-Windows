#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "err.h"
#include "outguess.h"
#include "win32/winutf8.h"
#include "i18n.h"

char *progname;

void
usage(void)
{

	fprintf(stderr, og_L("%s: <file>\n", "%s: <文件>\n"), progname);
}

void
histogram_simple(u_char *data, int bits)
{
	int i;
	int one = 0, zero = 0;

	for (i = 0; i < bits; i++)
		if (TEST_BIT(data, i))
			one++;
		else
			zero++;

	fprintf(stdout, og_L("Bits: %6d\n", "比特: %6d\n"), bits);
	fprintf(stdout, og_L("One:  %6d, %f\n", "置1:  %6d, %f\n"), one, (float)one/bits);
	fprintf(stdout, og_L("Zero: %6d, %f\n", "置0: %6d, %f\n"), zero, (float)zero/bits);
}

#define MAXRUNLEN	25

void
histogram_runlen(u_char *data, int bits)
{
	int buckets[MAXRUNLEN];
	int what, count, i;

	memset(buckets, 0, sizeof(buckets));
	what = TEST_BIT(data, 0);
	count = 1;
	for (i = 1; i < bits; i++) {
		if (TEST_BIT(data, i) != what) {
			if (count >= MAXRUNLEN)
				count = MAXRUNLEN - 1;
			buckets[count]++;

			what = TEST_BIT(data, i);
			count = 1;
		} else
			count++;
	}
	if (count >= MAXRUNLEN)
		count = MAXRUNLEN - 1;
	buckets[count]++;

	for (i = 1; i < MAXRUNLEN; i++) {
		fprintf(stdout, "%3d:  %6d, %f\n",
			i, buckets[i], (float)buckets[i]/bits);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fin;
	char *data;
	int bits, bytes, res;

	progname = argv[0];

#ifdef _WIN32
	/* 让命令行参数（如中文数据文件路径）按 UTF-8 解析，而非系统 ANSI 代码页 */
	win32_to_utf8_args(&argc, &argv);
#endif
	og_i18n_init(&argc, &argv);

	argv++;
	argc--;

	if (argc != 1) {
		usage();
		exit(1);
	}

	if ((fin = fopen_utf8(argv[0], "r")) == NULL)
		err(1, "fopen");

	if ((res = fread(&bits, sizeof(int), 1, fin)) != 1)
		err(1, "fread(1): %d", res);

	bits = ntohl(bits);
	bytes = (bits + 7) / 8;
	if ((data = malloc(bytes)) == NULL)
		err(1, "malloc");

	if ((res = fread(data, sizeof(char), bytes, fin)) != bytes)
		err(1, "fread(2): %d", res);

	histogram_simple(data, bits);
	histogram_runlen(data, bits);

	exit(0);
}
