#ifndef WEBCAM_H
#define WEBCAM_H

#include <stdint.h>
#include <stddef.h>

#define WEBCAM_OUT_W 128
#define WEBCAM_OUT_H 120

#define WEBCAM_MAX_BUFFERS 4

typedef struct WebcamBuffer {
	void *start;
	size_t length;
} WebcamBuffer;

typedef struct Webcam {
	int fd;
	uint8_t connected;

	uint32_t width;
	uint32_t height;
	uint32_t pixelformat;

	WebcamBuffer buffers[WEBCAM_MAX_BUFFERS];
	int n_buffers;
} Webcam;

int init_webcam (Webcam *cam);
void cleanup_webcam (Webcam *cam);
void webcam_get_frame (Webcam *cam, uint8_t out[WEBCAM_OUT_W * WEBCAM_OUT_H]);

#endif
