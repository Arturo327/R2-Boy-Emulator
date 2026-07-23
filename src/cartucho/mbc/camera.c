#include "cartucho/mbc/camera.h"
#include "frontend/webcam.h"
#include "gb.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

_Static_assert(WEBCAM_OUT_W == GBCAM_SENSOR_W && WEBCAM_OUT_H == GBCAM_SENSOR_H,
		"webcam output size must match the camera sensor size");

static inline int cam_clamp (int min, int value, int max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

static const float EDGE_ALPHA_LUT[8] = {
	0.50f, 0.75f, 1.00f, 1.25f, 2.00f, 3.00f, 4.00f, 5.00f
};

static uint8_t camera_matrix_process (CameraState *cam, int value, int x, int y)
{
	x &= 3;
	y &= 3;
	int base = 6 + (y * 4 + x) * 3;

	int r0 = cam->regs[base + 0];
	int r1 = cam->regs[base + 1];
	int r2 = cam->regs[base + 2];

	if (value < r0) return 0x00;
	if (value < r1) return 0x40;
	if (value < r2) return 0x80;
	return 0xC0;
}

typedef struct CamConfig {
	uint32_t p_bits;
	uint32_t m_bits;
	uint32_t n_bit;
	uint32_t vh_bits;
	uint32_t exposure;
	float edge_alpha;
	uint32_t e3_bit;
	uint32_t i_bit;
} CamConfig;

static void camera_decode_regs (CameraState *cam, CamConfig *cfg)
{
	cfg->p_bits = 0;
	cfg->m_bits = 0;
	switch ((cam->regs[0] >> 1) & 3)
	{
	case 0: cfg->p_bits = 0x00; cfg->m_bits = 0x01; break;
	case 1: cfg->p_bits = 0x01; cfg->m_bits = 0x00; break;
	case 2:
	case 3: cfg->p_bits = 0x01; cfg->m_bits = 0x02; break;
	}

	cfg->n_bit = (cam->regs[1] & 0x80) >> 7;
	cfg->vh_bits = (cam->regs[1] & 0x60) >> 5;
	cfg->exposure = cam->regs[3] | ((uint32_t)cam->regs[2] << 8);
	cfg->edge_alpha = EDGE_ALPHA_LUT[(cam->regs[4] & 0x70) >> 4];
	cfg->e3_bit = (cam->regs[4] & 0x80) >> 7;
	cfg->i_bit = (cam->regs[4] & 0x08) >> 3;
}

static void camera_capture_sensor (GB *gb, CameraState *cam, CamConfig *cfg)
{
	uint8_t frame[WEBCAM_OUT_W * WEBCAM_OUT_H];
	webcam_get_frame(&gb->webcam, frame);

	int i = 0;
	int row;
	for (int x = 0; x < GBCAM_SENSOR_W; x++) {
		row = 0;
		for (int y = 0; y < GBCAM_SENSOR_H; y++) {

			int value = frame[row + x];
			value = (value * (int)cfg->exposure) / 0x0300;
			value = 128 + ((value - 128) / 8);
			value = cam_clamp(0, value, 255);

			if (cfg->i_bit) value = 255 - value;

			cam->retina_buf[i++] = value - 128;
			row += WEBCAM_OUT_W;
		}
	}
}

static void camera_pm_combine (const int *src, int *dst, uint32_t p_bits, uint32_t m_bits)
{
	int i = 0;
	for (int x = 0; x < GBCAM_SENSOR_W; x++) {
		for (int y = 0; y < GBCAM_SENSOR_H; y++) {

			int px = src[i];
			int ms = src[i + (y < GBCAM_SENSOR_H - 1)];

			int value = 0;
			if (p_bits & 1) value += px;
			if (p_bits & 2) value += ms;
			if (m_bits & 1) value -= px;
			if (m_bits & 2) value -= ms;

			dst[i++] = cam_clamp(-128, value, 127);
		}
	}
}

static void camera_edge_1d (const int *src, int *dst, float edge_alpha)
{
	int i = 0;
	for (int x = 0; x < GBCAM_SENSOR_W; x++) {

		int mw_offset = (x > 0) ? -GBCAM_SENSOR_H : 0;
		int me_offset = (x < GBCAM_SENSOR_W - 1) ? GBCAM_SENSOR_H : 0;

		for (int y = 0; y < GBCAM_SENSOR_H; y++) {

			int mw = src[i + mw_offset];
			int me = src[i + me_offset];
			int px = src[i];

			int value = px + (int)((2 * px - mw - me) * edge_alpha);
			dst[i++] = cam_clamp(0, value, 255);
		}
	}
}

static void camera_edge_2d (const int *src, int *dst, float edge_alpha)
{
	int i = 0;
	for (int x = 0; x < GBCAM_SENSOR_W; x++) {

		int mw_offset = (x > 0) ? -GBCAM_SENSOR_H : 0;
		int me_offset = (x < GBCAM_SENSOR_W - 1) ? GBCAM_SENSOR_H : 0;

		for (int y = 0; y < GBCAM_SENSOR_H; y++) {

			int mn = src[i - (y > 0)];
			int ms = src[i + (y < GBCAM_SENSOR_H - 1)];
			int mw = src[i + mw_offset];
			int me = src[i + me_offset];
			int px = src[i];

			int value = px + (int)((4 * px - mw - me - mn - ms) * edge_alpha);
			dst[i++] = cam_clamp(-128, value, 127);
		}
	}
}

static void camera_apply_filter (CameraState *cam, uint32_t filtering_mode, CamConfig *cfg)
{
	switch (filtering_mode)
	{
	case 0x0:
		camera_pm_combine(cam->retina_buf, cam->retina_buf, cfg->p_bits, cfg->m_bits);
		break;

	case 0x2:
		camera_edge_1d(cam->retina_buf, cam->temp_buf, cfg->edge_alpha);
		camera_pm_combine(cam->temp_buf, cam->retina_buf, cfg->p_bits, cfg->m_bits);
		break;

	case 0xE:
		camera_edge_2d(cam->retina_buf, cam->temp_buf, cfg->edge_alpha);
		memcpy(cam->retina_buf, cam->temp_buf, sizeof(cam->retina_buf));
		break;

	case 0x1:
		memset(cam->retina_buf, 0, sizeof(cam->retina_buf));
		break;

	default:
		fprintf(stderr, "Camera: unsupported filtering mode 0x%X (regs %02X %02X %02X %02X %02X)\n",
			filtering_mode, cam->regs[0], cam->regs[1], cam->regs[2],
			cam->regs[3], cam->regs[4]);
		break;
	}
}

#define GBCAM_TILE_ROWS (GBCAM_H / 8)
#define GBCAM_TILE_COLS (GBCAM_W / 8)
#define GBCAM_TILES_BYTES (GBCAM_TILE_ROWS * GBCAM_TILE_COLS * 16)

static void camera_build_tiles (CameraState *cam, uint8_t tiles[GBCAM_TILE_ROWS][GBCAM_TILE_COLS][16])
{
	memset(tiles, 0, GBCAM_TILES_BYTES);
	int offset = GBCAM_SENSOR_EXTRA_LINES >> 1;

	for (int x = 0; x < GBCAM_W; x++) {

		uint8_t bit = 1 << (7 - (x & 7));
		int tile_x = x >> 3;
		int *src = &cam->retina_buf[offset];

		for (int y = 0; y < GBCAM_H; y++) {

			int value = *src + 128;
			int shade = camera_matrix_process(cam, value, x, y);
			uint8_t outcolor = 3 - (shade >> 6);

			uint8_t *tile = &tiles[y >> 3][tile_x][(y & 7) * 2];

			if (outcolor & 1) tile[0] |= bit;
			if (outcolor & 2) tile[1] |= bit;

			src++;
		}

		offset += GBCAM_SENSOR_H;
	}
}

static void camera_store_tiles (GB *gb, uint8_t tiles[GBCAM_TILE_ROWS][GBCAM_TILE_COLS][16])
{
	Cartucho *cart = &gb->memory.cart;

	pthread_mutex_lock(&gb->save.lock);
	if (cart->ram && cart->ram_size >= 0x100 + GBCAM_TILES_BYTES)
		memcpy(cart->ram + 0x100, tiles, GBCAM_TILES_BYTES);
	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}

static void camera_start_capture (GB *gb, CameraState *cam)
{
	CamConfig cfg;
	camera_decode_regs(cam, &cfg);

	uint32_t n_bit_timing = (cam->regs[1] & 0x80) ? 0 : 512;
	cam->capture_cycles_left = 32446u + n_bit_timing + 16u * cfg.exposure;

	camera_capture_sensor(gb, cam, &cfg);

	uint32_t filtering_mode = (cfg.n_bit << 3) | (cfg.vh_bits << 1) | cfg.e3_bit;
	camera_apply_filter(cam, filtering_mode, &cfg);

	uint8_t tiles[GBCAM_TILE_ROWS][GBCAM_TILE_COLS][16];
	camera_build_tiles(cam, tiles);
	camera_store_tiles(gb, tiles);
}

static uint8_t camera_read_reg (CameraState *cam, uint16_t addr)
{
	uint8_t off = addr & 0x7F;

	if (off == 0x00) {
		uint8_t busy = cam->regs[0] & 0x01;
		return (cam->regs[0] & 0x06) | busy;
	}
	return 0x00;
}

static void camera_write_reg (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	CameraState *cam = (CameraState *)cart->state;
	uint8_t off = addr & 0x7F;

	if (off >= GBCAM_NUM_REGS) return;

	if (off == 0x00) {
		uint8_t prev_bit0 = cam->regs[0] & 0x01;
		cam->regs[0] = val & 0x07;

		if ((val & 0x01) && !prev_bit0) {
			if (cam->capture_cycles_left == 0)
				camera_start_capture(gb, cam);
		}
		return;
	}

	cam->regs[off] = val;
}

int cam_init (Cartucho *cart)
{
	cart->state = malloc(sizeof(CameraState));
	if (!cart->state) return 0;
	memset(cart->state, 0, sizeof(CameraState));
	return 1;
}

void cam_free (Cartucho *cart)
{
	free(cart->state);
	cart->state = NULL;
}

uint8_t cam_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	uint32_t offset;

	if (addr < 0x4000) {
		offset = addr;
	} else {
		uint32_t bank = cart->rom_bank;
		if (cart->rom_banks) bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void cam_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x2000) {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x4000) {
		cart->rom_bank = val & 0x3F;

	} else if (addr < 0x6000) {
		cart->ram_bank = val;
	}
}

uint8_t cam_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	CameraState *cam = (CameraState *)cart->state;

	if (cart->ram_bank & 0x10)
		return camera_read_reg(cam, addr);

	if (cam->capture_cycles_left > 0) return 0x00;
	if (!cart->ram || cart->ram_size == 0) return 0xFF;

	uint32_t bank = cart->ram_bank & 0x0F;
	if (cart->ram_banks) bank &= (cart->ram_banks - 1);
	uint32_t offset = (bank << 13) + (addr - 0xA000);

	if (offset < cart->ram_size)
		return cart->ram[offset];
	return 0xFF;
}

void cam_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	CameraState *cam = (CameraState *)cart->state;

	if (cart->ram_bank & 0x10) {
		camera_write_reg(gb, addr, val);
		return;
	}

	if (!cart->ram_enabled || !cart->ram) return;
	if (cam->capture_cycles_left > 0) return;

	uint32_t bank = cart->ram_bank & 0x0F;
	if (cart->ram_banks) bank &= (cart->ram_banks - 1);
	uint32_t offset = (bank << 13) + (addr - 0xA000);

	pthread_mutex_lock(&gb->save.lock);
	if (offset < cart->ram_size)
		cart->ram[offset] = val;
	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}

void cam_step (GB *gb)
{
	Cartucho *cart = &gb->memory.cart;
	CameraState *cam = (CameraState *)cart->state;

	if (!(cam->regs[0] & 0x01)) return;

	if (cam->capture_cycles_left == 0) {
		cam->regs[0] &= ~0x01;
		return;
	}

	if (--cam->capture_cycles_left == 0)
		cam->regs[0] &= ~0x01;
}
