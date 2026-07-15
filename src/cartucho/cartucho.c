#include "cartucho/cartucho.h"
#include "cartucho/mbc1.h"
#include "cartucho/mbc2.h"
#include "cartucho/mbc3.h"
#include "cartucho/mbc5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void normalize_mbc (Cartucho *cart, uint8_t header_type) {
	cart->battery = 0;

	switch (header_type) {
		case 0x00: cart->mbc_type = MBC_NONE; break;

		case 0x01: cart->mbc_type = MBC1; break;
		case 0x02: cart->mbc_type = MBC1; break;
		case 0x03: cart->mbc_type = MBC1; cart->battery = 1; break;

		case 0x05: cart->mbc_type = MBC2; break;
		case 0x06: cart->mbc_type = MBC2; cart->battery = 1; break;

		case 0x08: cart->mbc_type = MBC_NONE; break;
		case 0x09: cart->mbc_type = MBC_NONE; cart->battery = 1; break;

		case 0x0F: cart->mbc_type = MBC3; cart->battery = 1; cart->has_rtc = 1; break;
		case 0x10: cart->mbc_type = MBC3; cart->battery = 1; cart->has_rtc = 1; break;
		case 0x11: cart->mbc_type = MBC3; break;
		case 0x12: cart->mbc_type = MBC3; break;
		case 0x13: cart->mbc_type = MBC3; cart->battery = 1; break;

		case 0x19: cart->mbc_type = MBC5; break;
		case 0x1A: cart->mbc_type = MBC5; break;
		case 0x1B: cart->mbc_type = MBC5; cart->battery = 1; break;
		case 0x1C: cart->mbc_type = MBC5; cart->has_rumble = 1; break;
		case 0x1D: cart->mbc_type = MBC5; cart->has_rumble = 1; break;
		case 0x1E: cart->mbc_type = MBC5; cart->has_rumble = 1; cart->battery = 1; break;

		default:
			fprintf(stderr, "Cartucho: Unknown MBC type (0x%02X), considering as ROM only\n", header_type);
			cart->mbc_type = MBC_NONE;
			break;
	}
}

static void select_mbc_fx (Cartucho *cart) {
	switch (cart->mbc_type) {
		case MBC_NONE:
			cart->read_rom = mbcNone_read_rom;
			cart->write_rom = mbcNone_write_rom;
			cart->read_ram = mbcNone_read_ram;
			cart->write_ram = mbcNone_write_ram;
			break;

		case MBC1:
			cart->read_rom = mbc1_read_rom;
			cart->write_rom = mbc1_write_rom;
			cart->read_ram = mbc1_read_ram;
			cart->write_ram = mbc1_write_ram;
			break;
		
		case MBC2:
			cart->read_rom = mbc2_read_rom;
			cart->write_rom = mbc2_write_rom;
			cart->read_ram = mbc2_read_ram;
			cart->write_ram = mbc2_write_ram;
			break;

		case MBC3:
			cart->read_rom = mbc3_read_rom;
			cart->write_rom = mbc3_write_rom;
			cart->read_ram = mbc3_read_ram;
			cart->write_ram = mbc3_write_ram;
			break;

		case MBC5:
			cart->read_rom = mbc5_read_rom;
			cart->write_rom = mbc5_write_rom;
			cart->read_ram = mbc5_read_ram;
			cart->write_ram = mbc5_write_ram;
			break;

		default:
			fprintf(stderr, "Cartridge: MBC%d unimplemented, using ROM-only (probably incorrect banking)\n", cart->mbc_type);
			cart->read_rom = mbcNone_read_rom;
			cart->write_rom = mbcNone_write_rom;
			cart->read_ram = mbcNone_read_ram;
			cart->write_ram = mbcNone_write_ram;
			break;
	}
}

static int is_multicart (Cartucho *cart)
{
	if (cart->mbc_type != MBC1 || cart->rom_size != 0x100000)
		return 0;

	// Logo de Nintendo
	const uint8_t logo[48] = {
		0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
		0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
		0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
		0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
		0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
		0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
	};

	uint32_t offset = (0x10 * 0x4000) + 0x0104;
	if (offset + sizeof(logo) <= cart->rom_size)
		return memcmp(cart->rom + offset, logo, sizeof(logo)) == 0;

	return 0;
}

static int is_mbc30 (Cartucho *cart) {
	if (cart->mbc_type != MBC3) return 0;
	return (cart->rom_banks > 128) || (cart->ram_size > 0x8000);
}

static int read_rom (Cartucho *cart, const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return 0;

	fseek(f, 0, SEEK_END);
	cart->rom_size = ftell(f);
	rewind(f);

	cart->rom = malloc(cart->rom_size);

	if (!cart->rom) {
		fclose(f);
		fprintf(stderr, "Not enough memory to load the ROM %s", filename);
		return 0;
	}

	int a = fread(cart->rom, 1, cart->rom_size, f);
	fclose(f);
	if (a != (int)cart->rom_size) return 0;

	if (cart->rom_size < 0x150) {
		fprintf(stderr, "ROM: file to small (%u bytes) for containing a valid header\n",
			(unsigned)cart->rom_size);
		return 0;
	}
	cart->rom_banks = cart->rom_size / 0x4000;
	return 1;
}

static void get_title (Cartucho *cart)
{
	uint8_t bytes = 16;
	if (cart->rom[0x0143] == 0x80 || cart->rom[0x0143] == 0xC0)
		bytes--;

	if (cart->rom[0x014B] == 0x33)
		bytes -= 4;

	memcpy(cart->title, cart->rom + 0x0134, bytes);

	cart->title[bytes] = '\0';
}

int load_rom (Cartucho *cart, const char *filename)
{
	cart->rom_bank = 1;
	cart->bank1 = 1;
	cart->rtc.latch_prev = 0xFF;
	cart->rtc.base = time(NULL);

	if (!read_rom(cart, filename)) return 0;

	normalize_mbc(cart, cart->rom[0x0147]);
	cart->multicart = is_multicart(cart);
	get_title(cart);

	uint8_t ram_type = cart->rom[0x0149];
	uint32_t ram_sizes[] = {0, 2*1024, 8*1024, 32*1024, 128*1024, 64*1024};
	cart->ram_size = (ram_type < 6) ? ram_sizes[ram_type] : 0;

	if (cart->mbc_type == MBC2)
		cart->ram_size = 512;

	cart->ram_banks = cart->ram_size / 0x2000;
	if (cart->ram_size) {
		cart->ram = calloc(1, cart->ram_size);
		if (cart->mbc_type == MBC_NONE)
			cart->ram_enabled = 1;
	}

	cart->mbc30 = is_mbc30(cart);

	select_mbc_fx(cart);
	printf("ROM: %s\n", filename);
	return 1;
}

static void make_sav_path (const char *romfile, char *out, size_t outsize) {
	size_t len = strlen(romfile);
	const char *dot = strrchr(romfile, '.');
	const char *slash = strrchr(romfile, '/');

	size_t base_len = len;
	if (dot && (!slash || dot > slash)) {
		base_len = (size_t)(dot - romfile);
	}

	if (base_len > outsize - 5) base_len = outsize - 5;

	memcpy(out, romfile, base_len);
	memcpy(out + base_len, ".sav", 5);
}

static inline int read_int64 (FILE *f, int64_t *v) {
	*v = 0;
	for (int i = 0; i < 8; i++) {
		int a = fgetc(f);
		if (a == EOF) return 0;
		*v |= (int64_t)((uint64_t)(uint8_t)a) << (i * 8);
	}
	return 1;
}

static inline int write_int64 (FILE *f, int64_t v) {
	for (int i = 0; i < 8; i++) {
		uint8_t a = (v >> (i * 8)) & 0xFF;
		if (fputc(a, f) == EOF) return 0;
	}
	return 1;
}

int load_sram (Cartucho *cart, const char *romfile)
{
	if (!cart->battery) return 0;
	if ((!cart->ram || cart->ram_size == 0) && !cart->has_rtc) return 0;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));

	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	if (cart->ram && cart->ram_size > 0) {
		int a = fread(cart->ram, 1, cart->ram_size, f);
		if (!a) {
			fprintf(stderr, "Could not load saved game %s\n", path);
			fclose(f);
			return 0;
		}
	}

	if (cart->has_rtc) {
		uint8_t buf[5];
		int64_t base;

		if (fread(buf, 1, 5, f) == 5 && read_int64(f, &base)) {
			cart->rtc.s = buf[0] & 0x3F;
			cart->rtc.m = buf[1] & 0x3F;
			cart->rtc.h = buf[2] & 0x1F;
			cart->rtc.d = ((uint16_t)(buf[4] & 0x01) << 8) | buf[3];
			cart->rtc.halt = (buf[4] & 0x40) != 0;
			cart->rtc.carry = (buf[4] & 0x80) != 0;
			cart->rtc.base = (time_t) base;

			cart->rtc.s_l = cart->rtc.s;
			cart->rtc.m_l = cart->rtc.m;
			cart->rtc.h_l = cart->rtc.h;
			cart->rtc.d_l = cart->rtc.d;
			cart->rtc.halt_l = cart->rtc.halt;
			cart->rtc.carry_l = cart->rtc.carry;
		} else {
			printf("No RTC data in %s, starting clock fresh\n", path);
		}
	}

	fclose(f);
	printf("Loaded game: %s\n", path);
	return 1;
}

int save_sram (Cartucho *cart, const char *romfile)
{
	if (!cart->battery) return 0;
	if ((!cart->ram || cart->ram_size == 0) && !cart->has_rtc) return 0;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Could not save game %s\n", path);
		return 0;
	}

	if (cart->ram && cart->ram_size > 0)
		fwrite(cart->ram, 1, cart->ram_size, f);

	if (cart->has_rtc) {
		uint8_t dh = ((cart->rtc.d >> 8) & 0x01)
			| (cart->rtc.halt  ? 0x40 : 0)
			| (cart->rtc.carry ? 0x80 : 0);
		uint8_t buf[5] = { cart->rtc.s, cart->rtc.m, cart->rtc.h,
				(uint8_t)(cart->rtc.d & 0xFF), dh };
		int64_t base = (int64_t) cart->rtc.base;

		fwrite(buf, 1, 5, f);
		(void)write_int64(f, base);
	}

	fclose(f);
	printf("Saved game: %s\n", path);
	return 1;
}

void free_cart (Cartucho *cart)
{
	free(cart->rom);
	free(cart->ram);
}
