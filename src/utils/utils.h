#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

size_t path_base_len (const char *path);
size_t path_copy_base (const char *path, char *out, size_t outsize);
size_t path_with_suffix (const char *path, const char *suffix, char *out, size_t outsize);

int file_exists (const char *path);
int next_numbered_file (const char *prefix, const char *ext, int *counter,
			char *out, size_t outsize);
int write_bmp (const char *path, const uint32_t *pixels, int w, int h);

#endif
