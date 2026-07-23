#include "frontend/webcam.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <setjmp.h>
#include <jpeglib.h>

#define WEBCAM_DEVICE "/dev/video0"

static int xioctl (int fd, unsigned long req, void *arg)
{
	int r;
	do {
		r = ioctl(fd, req, arg);
	} while (r == -1 && errno == EINTR);
	return r;
}

static void fill_synthetic_frame (uint8_t out[WEBCAM_OUT_W * WEBCAM_OUT_H])
{
	static uint32_t phase = 0;
	phase += 3;

	for (int y = 0; y < WEBCAM_OUT_H; y++) {
		int cy = y - WEBCAM_OUT_H / 2;
		for (int x = 0; x < WEBCAM_OUT_W; x++) {
			int cx = x - WEBCAM_OUT_W / 2;
			int dist = cx * cx + cy * cy;
			out[y * WEBCAM_OUT_W + x] = (uint8_t)((dist / 40 + phase) & 0xFF);
		}
	}
}

void cleanup_webcam (Webcam *cam)
{
	if (cam->fd >= 0) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		xioctl(cam->fd, VIDIOC_STREAMOFF, &type);
	}

	for (int i = 0; i < cam->n_buffers; i++) {
		if (cam->buffers[i].start && cam->buffers[i].start != MAP_FAILED)
			munmap(cam->buffers[i].start, cam->buffers[i].length);
		cam->buffers[i].start = NULL;
	}
	cam->n_buffers = 0;

	if (cam->fd >= 0) close(cam->fd);
	cam->fd = -1;
	cam->connected = 0;
}

static int webcam_open_device (Webcam *cam)
{
	cam->fd = open(WEBCAM_DEVICE, O_RDWR | O_NONBLOCK, 0);
	if (cam->fd < 0) return 0;

	struct v4l2_capability cap;
	memset(&cap, 0, sizeof(cap));
	if (xioctl(cam->fd, VIDIOC_QUERYCAP, &cap) < 0) return 0;

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
			!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "Webcam: %s doesn't support streaming capture\n", WEBCAM_DEVICE);
		return 0;
	}
	return 1;
}

static inline int is_jpeg_fourcc (uint32_t fourcc)
{
	return fourcc == V4L2_PIX_FMT_MJPEG || fourcc == V4L2_PIX_FMT_JPEG;
}

static int try_format (Webcam *cam, uint32_t want_fourcc, struct v4l2_format *fmt)
{
	memset(fmt, 0, sizeof(*fmt));
	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->fmt.pix.width = 640;
	fmt->fmt.pix.height = 480;
	fmt->fmt.pix.pixelformat = want_fourcc;
	fmt->fmt.pix.field = V4L2_FIELD_ANY;

	return xioctl(cam->fd, VIDIOC_S_FMT, fmt) >= 0;
}

static int webcam_setup_format (Webcam *cam)
{
	struct v4l2_format fmt;

	if (try_format(cam, V4L2_PIX_FMT_YUYV, &fmt) && fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
		cam->pixelformat = V4L2_PIX_FMT_YUYV;

	} else if (try_format(cam, V4L2_PIX_FMT_MJPEG, &fmt) && is_jpeg_fourcc(fmt.fmt.pix.pixelformat)) {
		cam->pixelformat = fmt.fmt.pix.pixelformat;

	} else if (try_format(cam, V4L2_PIX_FMT_JPEG, &fmt) && is_jpeg_fourcc(fmt.fmt.pix.pixelformat)) {
		cam->pixelformat = fmt.fmt.pix.pixelformat;

	} else {
		fprintf(stderr, "Webcam: camera doesn't offer YUYV or JPEG/MJPEG output (this simple driver doesn't decode other formats)\n");
		return 0;
	}

	cam->width = fmt.fmt.pix.width;
	cam->height = fmt.fmt.pix.height;
	return 1;
}

static int webcam_setup_buffers (Webcam *cam)
{
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = WEBCAM_MAX_BUFFERS;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (xioctl(cam->fd, VIDIOC_REQBUFS, &req) < 0) return 0;
	if (req.count < 2) return 0;

	cam->n_buffers = (int)req.count;
	if (cam->n_buffers > WEBCAM_MAX_BUFFERS) cam->n_buffers = WEBCAM_MAX_BUFFERS;

	for (int i = 0; i < cam->n_buffers; i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = (unsigned)i;

		if (xioctl(cam->fd, VIDIOC_QUERYBUF, &buf) < 0) return 0;

		cam->buffers[i].length = buf.length;
		cam->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, cam->fd, buf.m.offset);
		if (cam->buffers[i].start == MAP_FAILED) return 0;
	}

	for (int i = 0; i < cam->n_buffers; i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = (unsigned)i;
		if (xioctl(cam->fd, VIDIOC_QBUF, &buf) < 0) return 0;
	}

	return 1;
}

int init_webcam (Webcam *cam)
{
	memset(cam, 0, sizeof(Webcam));
	cam->fd = -1;

	if (!webcam_open_device(cam)) {
		cleanup_webcam(cam);
		return 0;
	}
	if (!webcam_setup_format(cam) || !webcam_setup_buffers(cam)) {
		cleanup_webcam(cam);
		return 0;
	}

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(cam->fd, VIDIOC_STREAMON, &type) < 0) {
		cleanup_webcam(cam);
		return 0;
	}

	cam->connected = 1;
	return 1;
}

static int webcam_grab_raw (Webcam *cam, struct v4l2_buffer *out_buf)
{
	struct pollfd pfd = {
		.fd = cam->fd,
		.events = POLLIN
	};
	if (poll(&pfd, 1, 500) <= 0) return 0;

	memset(out_buf, 0, sizeof(*out_buf));
	out_buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	out_buf->memory = V4L2_MEMORY_MMAP;

	return xioctl(cam->fd, VIDIOC_DQBUF, out_buf) >= 0;
}

static void yuyv_downscale_gray (const uint8_t *src, uint32_t sw, uint32_t sh, uint8_t out[WEBCAM_OUT_W * WEBCAM_OUT_H])
{
	for (int y = 0; y < WEBCAM_OUT_H; y++) {
		uint32_t sy = (uint32_t)((uint64_t)y * sh / WEBCAM_OUT_H);
		for (int x = 0; x < WEBCAM_OUT_W; x++) {
			uint32_t sx = (uint32_t)((uint64_t)x * sw / WEBCAM_OUT_W);
			out[y * WEBCAM_OUT_W + x] = src[(sy * sw + sx) * 2];
		}
	}
}

struct webcam_jpeg_error {
	struct jpeg_error_mgr pub;
	jmp_buf jmp;
};

static void webcam_jpeg_error_exit (j_common_ptr cinfo)
{
	struct webcam_jpeg_error *err = (struct webcam_jpeg_error *)cinfo->err;
	char msg[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, msg);
	fprintf(stderr, "Webcam: JPEG decode error: %s\n", msg);
	longjmp(err->jmp, 1);
}

static void webcam_jpeg_emit_message (j_common_ptr cinfo, int msg_level)
{
	(void)cinfo;
	(void)msg_level;
}

static int decode_mjpeg_gray (const uint8_t *jpeg_data, size_t jpeg_size,
		uint8_t out[WEBCAM_OUT_W * WEBCAM_OUT_H])
{
	if (!jpeg_data || jpeg_size == 0) return 0;

	struct jpeg_decompress_struct cinfo;
	struct webcam_jpeg_error jerr;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = webcam_jpeg_error_exit;
	jerr.pub.emit_message = webcam_jpeg_emit_message;

	if (setjmp(jerr.jmp)) {
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, jpeg_data, (unsigned long)jpeg_size);

	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	cinfo.out_color_space = JCS_GRAYSCALE;
	jpeg_start_decompress(&cinfo);

	uint32_t sw = cinfo.output_width;
	uint32_t sh = cinfo.output_height;

	if (cinfo.output_components != 1 || sw == 0 || sh == 0) {
		jpeg_abort_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	uint8_t *rows = malloc((size_t)sw * (size_t)sh);
	if (!rows) {
		jpeg_abort_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	while (cinfo.output_scanline < cinfo.output_height) {
		uint8_t *row_ptr = rows + (size_t)cinfo.output_scanline * sw;
		jpeg_read_scanlines(&cinfo, &row_ptr, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	for (int y = 0; y < WEBCAM_OUT_H; y++) {
		uint32_t sy = (uint32_t)((uint64_t)y * sh / WEBCAM_OUT_H);
		for (int x = 0; x < WEBCAM_OUT_W; x++) {
			uint32_t sx = (uint32_t)((uint64_t)x * sw / WEBCAM_OUT_W);
			out[y * WEBCAM_OUT_W + x] = rows[(size_t)sy * sw + sx];
		}
	}

	free(rows);
	return 1;
}

void webcam_get_frame (Webcam *cam, uint8_t out[WEBCAM_OUT_W * WEBCAM_OUT_H])
{
	if (!cam->connected) {
		fill_synthetic_frame(out);
		return;
	}

	struct v4l2_buffer buf;
	if (!webcam_grab_raw(cam, &buf)) {
		fill_synthetic_frame(out);
		return;
	}

	int ok;
	if (is_jpeg_fourcc(cam->pixelformat)) {
		ok = decode_mjpeg_gray((const uint8_t *)cam->buffers[buf.index].start, buf.bytesused, out);
	} else {
		yuyv_downscale_gray((const uint8_t *)cam->buffers[buf.index].start, cam->width, cam->height, out);
		ok = 1;
	}

	xioctl(cam->fd, VIDIOC_QBUF, &buf);

	if (!ok) fill_synthetic_frame(out);
}
