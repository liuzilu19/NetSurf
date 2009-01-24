/*
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 * Copyright 2008 James Bursa <james@netsurf-browser.org>
 *
 * This file is part of NetSurf's libnsbmp, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../libnsbmp.h"

#define BYTES_PER_PIXEL 4
#define TRANSPARENT_COLOR 0xffffffff

unsigned char *load_file(const char *path, size_t *data_size);
void warning(const char *context, bmp_result code);
void *bitmap_create(int width, int height, unsigned int state);
void bitmap_set_suspendable(void *bitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word));
void invalidate(void *bitmap, void *private_word);
unsigned char *bitmap_get_buffer(void *bitmap);
size_t bitmap_get_bpp(void *bitmap);
void bitmap_destroy(void *bitmap);


int main(int argc, char *argv[])
{
	bmp_bitmap_callback_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_set_suspendable,
		bitmap_get_buffer,
		bitmap_get_bpp
	};
	bmp_result code;
	bmp_image bmp;
	size_t size;
	unsigned short res = 0;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s image.bmp\n", argv[0]);
		return 1;
	}

	/* create our bmp image */
	bmp_create(&bmp, &bitmap_callbacks);

	/* load file into memory */
	unsigned char *data = load_file(argv[1], &size);

	/* analyse the BMP */
	code = bmp_analyse(&bmp, size, data);
	if (code != BMP_OK) {
		warning("bmp_analyse", code);
		res = 1;
		goto cleanup;
	}

	/* decode the image */
	code = bmp_decode(&bmp);
	/* code = bmp_decode_trans(&bmp, TRANSPARENT_COLOR); */
	if (code != BMP_OK) {
		warning("bmp_decode", code);
		/* allow partially decoded images */
		if (code != BMP_INSUFFICIENT_DATA) {
			res = 1;
			goto cleanup;
		}
	}

	printf("P3\n");
	printf("# %s\n", argv[1]);
	printf("# width                %u \n", bmp.width);
	printf("# height               %u \n", bmp.height);
	printf("%u %u 256\n", bmp.width, bmp.height);

	{
		uint16_t row, col;
		uint8_t *image;
		image = (uint8_t *) bmp.bitmap;
		for (row = 0; row != bmp.height; row++) {
			for (col = 0; col != bmp.width; col++) {
				size_t z = (row * bmp.width + col) * BYTES_PER_PIXEL;
				printf("%u %u %u ",	image[z],
							image[z + 1],
							image[z + 2]);
			}
			printf("\n");
		}
	}

cleanup:
	/* clean up */
	bmp_finalise(&bmp);
	free(data);

	return res;
}


unsigned char *load_file(const char *path, size_t *data_size)
{
	FILE *fd;
	struct stat sb;
	unsigned char *buffer;
	size_t size;
	size_t n;

	fd = fopen(path, "rb");
	if (!fd) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	if (stat(path, &sb)) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	size = sb.st_size;

	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %lld bytes\n",
				(long long) size);
		exit(EXIT_FAILURE);
	}

	n = fread(buffer, 1, size, fd);
	if (n != size) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	fclose(fd);

	*data_size = size;
	return buffer;
}


void warning(const char *context, bmp_result code)
{
	fprintf(stderr, "%s failed: ", context);
	switch (code) {
		case BMP_INSUFFICIENT_MEMORY:
			fprintf(stderr, "BMP_INSUFFICIENT_MEMORY");
			break;
		case BMP_INSUFFICIENT_DATA:
			fprintf(stderr, "BMP_INSUFFICIENT_DATA");
			break;
		case BMP_DATA_ERROR:
			fprintf(stderr, "BMP_DATA_ERROR");
			break;
		default:
			fprintf(stderr, "unknown code %i", code);
			break;
	}
	fprintf(stderr, "\n");
}


void *bitmap_create(int width, int height, unsigned int state)
{
	(void) state;  /* unused */
	return calloc(width * height, BYTES_PER_PIXEL);
}


void bitmap_set_suspendable(void *bitmap, void *private_word,
			     void (*invalidate)(void *bitmap, void *private_word))
{
	(void) bitmap;  /* unused */
	(void) private_word;  /* unused */
	(void) invalidate;  /* unused */
}


void invalidate(void *bitmap, void *private_word)
{
	(void) bitmap;  /* unused */
	(void) private_word;  /* unused */
}


unsigned char *bitmap_get_buffer(void *bitmap)
{
	assert(bitmap);
	return bitmap;
}


size_t bitmap_get_bpp(void *bitmap)
{
	(void) bitmap;  /* unused */
	return BYTES_PER_PIXEL;
}


void bitmap_destroy(void *bitmap)
{
	assert(bitmap);
	free(bitmap);
}

