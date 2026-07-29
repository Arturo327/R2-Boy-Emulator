#include "utils/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t path_base_len (const char *path)
{
	if (!path) return 0;

	size_t len = strlen(path);
	const char *dot = strrchr(path, '.');
	const char *slash = strrchr(path, '/');

	if (dot && (!slash || dot > slash))
		return (size_t)(dot - path);

	return len;
}

size_t path_copy_base (const char *path, char *out, size_t outsize)
{
	if (!path || !out || outsize == 0) return 0;

	size_t base_len = path_base_len(path);
	if (base_len > outsize - 1)
		base_len = outsize - 1;

	memcpy(out, path, base_len);
	out[base_len] = '\0';
	return base_len;
}

size_t path_with_suffix (const char *path, const char *suffix, char *out, size_t outsize)
{
	if (!path || !suffix || !out || outsize == 0) return 0;

	size_t base_len = path_base_len(path);
	size_t suffix_len = strlen(suffix);

	if (base_len + suffix_len + 1 > outsize) return 0;

	memcpy(out, path, base_len);
	memcpy(out + base_len, suffix, suffix_len + 1);
	return base_len + suffix_len;
}

int file_exists (const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	fclose(f);
	return 1;
}

int next_numbered_file (const char *prefix, const char *ext, int *counter,
			char *out, size_t outsize)
{
	if (!prefix || !ext || !counter || !out || outsize == 0) return 0;

	for (;;) {
		int n = ++(*counter);
		int len = snprintf(out, outsize, "%s_%03d.%s", prefix, n, ext);
		if (len < 0 || (size_t)len >= outsize) return 0;
		if (!file_exists(out)) return 1;
	}
}

static void build_bmp_header (uint8_t header[54], int w, int h, uint32_t data_size)
{
	uint32_t file_size = 54u + data_size;

	memset(header, 0, 54);
	header[0] = 'B'; header[1] = 'M';
	header[2] = (uint8_t)(file_size);
	header[3] = (uint8_t)(file_size >> 8);
	header[4] = (uint8_t)(file_size >> 16);
	header[5] = (uint8_t)(file_size >> 24);
	header[10] = 54;
	header[14] = 40;
	header[18] = (uint8_t)(w);
	header[19] = (uint8_t)(w >> 8);
	header[20] = (uint8_t)(w >> 16);
	header[21] = (uint8_t)(w >> 24);
	header[22] = (uint8_t)(h);
	header[23] = (uint8_t)(h >> 8);
	header[24] = (uint8_t)(h >> 16);
	header[25] = (uint8_t)(h >> 24);
	header[26] = 1;
	header[28] = 24;
	header[34] = (uint8_t)(data_size);
	header[35] = (uint8_t)(data_size >> 8);
	header[36] = (uint8_t)(data_size >> 16);
	header[37] = (uint8_t)(data_size >> 24);
}

int write_bmp (const char *path, const uint32_t *pixels, int w, int h)
{
	if (!path || !pixels || w <= 0 || h <= 0) return 0;

	int row_bytes = w * 3;
	int pad = (4 - (row_bytes % 4)) % 4;
	size_t stride = (size_t)(row_bytes + pad);
	size_t data_size = stride * (size_t)h;

	uint8_t *data = malloc(data_size);
	if (!data) return 0;

	for (int y = 0; y < h; y++) {
		const uint32_t *src = pixels + (size_t)(h - 1 - y) * (size_t)w;
		uint8_t *dst = data + (size_t)y * stride;

		for (int x = 0; x < w; x++) {
			uint32_t px = src[x];
			dst[x * 3 + 0] = (uint8_t)(px & 0xFF);
			dst[x * 3 + 1] = (uint8_t)((px >> 8) & 0xFF);
			dst[x * 3 + 2] = (uint8_t)((px >> 16) & 0xFF);
		}
		if (pad) memset(dst + row_bytes, 0, (size_t)pad);
	}

	uint8_t header[54];
	build_bmp_header(header, w, h, (uint32_t)data_size);

	FILE *f = fopen(path, "wb");
	if (!f) {
		free(data);
		return 0;
	}

	int ok = fwrite(header, 1, sizeof(header), f) == sizeof(header)
		&& fwrite(data, 1, data_size, f) == data_size;

	fclose(f);
	free(data);
	return ok;
}
