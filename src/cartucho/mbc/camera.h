#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>

#define GBCAM_SENSOR_EXTRA_LINES 8
#define GBCAM_SENSOR_W 128
#define GBCAM_SENSOR_H (112 + GBCAM_SENSOR_EXTRA_LINES)

#define GBCAM_W 128
#define GBCAM_H 112

#define GBCAM_NUM_REGS 0x36

typedef struct GB GB;
typedef struct Cartucho Cartucho;

typedef struct CameraState {
	uint8_t regs[GBCAM_NUM_REGS];
	uint32_t capture_cycles_left;

	int retina_buf[GBCAM_SENSOR_W * GBCAM_SENSOR_H];
	int temp_buf[GBCAM_SENSOR_W * GBCAM_SENSOR_H];
} CameraState;

int cam_init (Cartucho *cart);
void cam_free (Cartucho *cart);

uint8_t cam_read_rom (GB *gb, uint16_t addr);
void cam_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t cam_read_ram (GB *gb, uint16_t addr);
void cam_write_ram (GB *gb, uint16_t addr, uint8_t val);

void cam_step (GB *gb);

#endif
