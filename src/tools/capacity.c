#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../config.h"
#include "../outguess.h"
#include "../pnm.h"
#include "../jpg.h"
#include "../win32/winutf8.h"
#include "../i18n.h"

int steg_foil;
int steg_foilfail;
int steg_stat;

void *checkedmalloc(size_t size)
{
	void *memory = malloc(size);
	if (!memory) {
		fprintf(stderr, og_L("Out of memory.\n", "内存不足。\n"));
		exit(1);
	}
	return memory;
}

static void usage(const char *program)
{
	fprintf(stderr, og_L("Usage: %s [-e] [-p quality] image.jpg\n", "用法: %s [-e] [-p 质量] image.jpg\n"), program);
	fprintf(stderr, og_L("  -e              compute capacity with error correction\n", "  -e              计算含纠错码的容量\n"));
	fprintf(stderr, og_L("  -p quality    JPEG quality between 75 and 100 (default: 75)\n", "  -p 质量      JPEG 质量 75 到 100（默认 75）\n"));
}

int main(int argc, char **argv)
{
	FILE *input;
	image *jpegImage;
	bitmap bitmap;
	int errorCorrection = 0;
	char *quality = NULL;
	int option;
	char *end;
	long qualityValue;

#ifdef _WIN32
	/* 让命令行参数（如中文图片路径）按 UTF-8 解析，而非系统 ANSI 代码页 */
	win32_to_utf8_args(&argc, &argv);
#endif
	og_i18n_init(&argc, &argv);

	while ((option = getopt(argc, argv, "ep:")) != -1) {
		if (option == 'e')
			errorCorrection = 1;
		else if (option == 'p')
			quality = optarg;
		else {
			usage(argv[0]);
			return 2;
		}
	}
	if (argc - optind != 1) {
		usage(argv[0]);
		return 2;
	}

	if (quality)
	{
		errno = 0;
		qualityValue = strtol(quality, &end, 10);
		if (errno || *quality == '\0' || *end != '\0' || qualityValue < 75 || qualityValue > 100) {
			fprintf(stderr, og_L("JPEG quality must be between 75 and 100.\n", "JPEG 质量必须在 75 到 100 之间。\n"));
			return 2;
		}
		init_JPEG_handler(quality);
	}
	else
		init_JPEG_handler(NULL);
	input = fopen_utf8(argv[optind], "rb");
	if (!input) {
		perror(argv[optind]);
		return 1;
	}
	jpegImage = read_JPEG_file(input);
	fclose(input);
	if (!jpegImage) {
		fprintf(stderr, og_L("Could not read the JPEG.\n", "无法读取 JPEG。\n"));
		return 1;
	}

	memset(&bitmap, 0, sizeof(bitmap));
	bitmap_from_jpg(&bitmap, jpegImage, 0);
	printf(og_L("Image: %s\n", "图像: %s\n"), argv[optind]);
	printf(og_L("Usable bits: %d\n", "可用比特: %d\n"), bitmap.bits);
	printf(og_L("Capacity without error correction: %d bytes\n", "不含纠错码的容量: %d bytes\n"), bitmap.bits / 8);
	int estimatedBits = preserve_jpg(&bitmap, -1);
	if (estimatedBits >= 0)
		printf(og_L("Capacity with error correction: %d bytes\n", "含纠错码的容量: %d bytes\n"), estimatedBits / 8);
	else
		printf(og_L("Capacity with error correction: unavailable\n", "含纠错码的容量: 不可用\n"));
	printf(og_L("Recommendation: %s\n", "建议: %s\n"), errorCorrection
		? (estimatedBits >= 0 ? og_L("Use -e; the message must fit within ECC capacity.", "使用 -e；消息必须能放入 ECC 容量。") : og_L("Avoid -e for this image.", "本图请避免使用 -e。"))
		: og_L("Use -e for better error tolerance when capacity allows.", "容量允许时，使用 -e 可获得更好的容错。"));
	return 0;
}
