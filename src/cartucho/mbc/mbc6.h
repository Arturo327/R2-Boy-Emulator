#ifndef MBC6_H
#define MBC6_H

#include <stdint.h>

typedef struct GB GB;
typedef struct Cartucho Cartucho;

#define MBC6_FLASH_SIZE 0x100000u
#define MBC6_SECTOR_SIZE 0x20000u
#define MBC6_SECTOR_MASK (MBC6_SECTOR_SIZE - 1)

enum {
	MF_IDLE = 0,
	MF_UNLOCK1,
	MF_UNLOCK2,
	MF_ERASE_UNLOCK1, 
	MF_ERASE_UNLOCK2, 
	MF_ERASE_UNLOCK3, 
	MF_PROTECT_UNLOCK1,
	MF_PROTECT_UNLOCK2,
	MF_PROTECT_UNLOCK3,
	MF_HIDDEN_UNLOCK1,
	MF_HIDDEN_UNLOCK2,
	MF_HIDDEN_UNLOCK3,
	MF_ID_MODE,
	MF_PROGRAM,
	MF_READ_HIDDEN,
	MF_STATUS
};

typedef struct MBC6Flash {
	uint8_t *data;
	uint8_t hidden[256];
	uint8_t sector0_locked;
	uint8_t write_enable;

	uint8_t state;
	uint8_t status;

	uint32_t program_addr;
	uint8_t program_buf[128];
	uint16_t program_len;
	uint8_t program_hidden;
} MBC6Flash;

typedef struct MBC6State {
	uint8_t rom_bank_a, rom_bank_b;
	uint8_t flash_sel_a, flash_sel_b;
	uint8_t ram_bank_a, ram_bank_b;
	uint8_t ram_enabled;
	uint8_t flash_enabled;
	uint8_t flash_write_enabled;
	MBC6Flash flash;
} MBC6State;

int mbc6_init (Cartucho *cart);
void mbc6_free (Cartucho *cart);

uint8_t mbc6_read_rom (GB *gb, uint16_t addr);
void mbc6_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc6_read_ram (GB *gb, uint16_t addr);
void mbc6_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
