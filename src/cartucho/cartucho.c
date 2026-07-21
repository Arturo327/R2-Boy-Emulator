#include "cartucho/cartucho.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void normalize_mbc (Cartucho *cart, uint8_t header_type) {
	cart->battery = 0;

	switch (header_type)
	{
	case 0x00: cart->mbc_type = MBC_NONE; break;

	case 0x01: cart->mbc_type = MBC1; break;
	case 0x02: cart->mbc_type = MBC1; break;
	case 0x03: cart->mbc_type = MBC1; cart->battery = 1; break;

	case 0x05: cart->mbc_type = MBC2; break;
	case 0x06: cart->mbc_type = MBC2; cart->battery = 1; break;

	case 0x08: cart->mbc_type = MBC_NONE; break;
	case 0x09: cart->mbc_type = MBC_NONE; cart->battery = 1; break;

	case 0x0B: cart->mbc_type = MMM01; break;
	case 0x0C: cart->mbc_type = MMM01; break;
	case 0x0D: cart->mbc_type = MMM01; cart->battery = 1; break;

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

	case 0x20: cart->mbc_type = MBC6; break;
	case 0x22: cart->mbc_type = MBC7; cart->battery = 1; break;

	case 0xFC: cart->mbc_type = HUC1; cart->battery = 1; break;
	case 0xFF: cart->mbc_type = HUC1; cart->battery = 1; break;

	default:
		fprintf(stderr, "Cartucho: Unknown MBC type (0x%02X), considering as ROM only\n", header_type);
		cart->mbc_type = MBC_NONE;
		break;
	}
}

#define LOAD_FX(type)			\
cart->read_rom = type##_read_rom;	\
cart->write_rom = type##_write_rom;	\
cart->read_ram = type##_read_ram;	\
cart->write_ram = type##_write_ram;	\
break;

static void select_mbc_fx (Cartucho *cart) {
	switch (cart->mbc_type)
	{
	case MBC_NONE:	LOAD_FX(mbcNone)
	case MBC1:	LOAD_FX(mbc1)
	case MBC2:	LOAD_FX(mbc2)
	case MBC3:	LOAD_FX(mbc3)
	case MBC5:	LOAD_FX(mbc5)
	case MBC6:	LOAD_FX(mbc6)
	case MBC7:	LOAD_FX(mbc7)
	case M161:	LOAD_FX(m161)
	case MMM01:	LOAD_FX(mmm01)
	case HUC1:	LOAD_FX(huc1)
	default:
		fprintf(stderr, "Cartridge: Current MBC unimplemented, using ROM-only (probably incorrect banking)\n");
		LOAD_FX(mbcNone)
	}
}

#undef LOAD_FX

const uint8_t NINTENDO_LOGO[48] = {
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
	0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
	0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
	0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

static int valid_logo (Cartucho *cart, uint32_t header_offset)
{
	uint32_t logo_off = header_offset + 0x0104;
	if (logo_off + sizeof(NINTENDO_LOGO) > cart->rom_size) return 0;
	return memcmp(cart->rom + logo_off, NINTENDO_LOGO, sizeof(NINTENDO_LOGO)) == 0;
}

static uint32_t find_header_base (Cartucho *cart)
{
	if (cart->rom_size <= 0x8000) return 0;

	uint32_t candidate = cart->rom_size - 0x8000;
	uint8_t type = cart->rom[candidate + 0x0147];

	if ((type == 0x0B || type == 0x0C || type == 0x0D) && valid_logo(cart, candidate))
		return candidate;

	return 0;
}

static int is_multicart (Cartucho *cart)
{
	if (cart->mbc_type != MBC1 || cart->rom_size != 0x100000)
		return 0;
	return valid_logo(cart, 0x40000);
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
	if (cart->rom_size <= 0) {
		fclose(f);
		return 0;
	}
	rewind(f);

	cart->rom = malloc(cart->rom_size);

	if (!cart->rom) {
		fclose(f);
		fprintf(stderr, "Not enough memory to load the ROM %s\n", filename);
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

static void get_title (Cartucho *cart, uint32_t header_base)
{
	uint8_t bytes = 16;
	if (cart->rom[header_base + 0x0143] == 0x80 || cart->rom[header_base + 0x0143] == 0xC0)
		bytes--;

	if (cart->rom[header_base + 0x014B] == 0x33)
		bytes -= 4;

	memcpy(cart->title, cart->rom + header_base + 0x0134, bytes);
	cart->title[bytes] = '\0';
}

static void detect_m161 (Cartucho *cart)
{
	if ((cart->mbc_type != MBC1 && cart->mbc_type != MBC3) || cart->rom_size != 0x40000)
		return;

	int game_count = 0;
	uint32_t offset = 0x8000;
	for (int i = 1; i < 8; i++) {
		if (valid_logo(cart, offset))
			game_count++;
		offset += 0x8000;
	}
	if (game_count == 0) return;

	cart->mbc_type = M161;
	cart->rom_bank = 0;
	cart->mbc_mode = 0;

	cart->battery = 0;
	cart->has_rtc = 0;
}

static void parse_cart_header (Cartucho *cart, uint32_t header_base)
{
	normalize_mbc(cart, cart->rom[0x0147 + header_base]);
	cart->multicart = is_multicart(cart);
	get_title(cart, header_base);
	detect_m161(cart);
}

static void get_cart_ram_size (Cartucho *cart, uint32_t header_base)
{
	uint8_t ram_type = cart->rom[header_base + 0x0149];
	uint32_t ram_sizes[] = {0, 2*1024, 8*1024, 32*1024, 128*1024, 64*1024};
	cart->ram_size = (ram_type < 6) ? ram_sizes[ram_type] : 0;

	if (cart->mbc_type == MBC2)
		cart->ram_size = 512;

	if (cart->mbc_type == MBC7)
		cart->ram_size = 256;

	if (cart->mbc_type == M161)
		cart->ram_size = 0;
}

static int cart_alloc_ram (Cartucho *cart)
{
	if (!cart->ram_size) return 1;

	cart->ram = calloc(1, cart->ram_size);
	if (!cart->ram) {
		fprintf(stderr, "Cartucho: not enough memory for the cartridge RAM\n");
		return 0;
	}

	cart->ram_banks = cart->ram_size / 0x2000;

	if (cart->mbc_type == MBC_NONE)
		cart->ram_enabled = 1;

	if (cart->mbc_type == MBC7 && cart->ram)
		memset(cart->ram, 0xFF, cart->ram_size);

	return 1;
}

static int init_special_mbcs (Cartucho *cart)
{
	if (cart->mbc_type == MBC6 && !mbc6_init(cart)) {
		fprintf(stderr, "Cartucho: not enough memory for the MBC6\n");
		return 0;
	}
	if (cart->mbc_type == MBC7 && !mbc7_init(cart)) {
		fprintf(stderr, "Cartucho: not enough memory for the MBC7\n");
		return 0;
	}
	if (cart->mbc_type == MMM01 && !mmm01_init(cart)) {
		fprintf(stderr, "Cartucho: not enough memory for the MMM01\n");
		return 0;
	}
	if (cart->has_rtc && !rtc_init(cart)) {
		fprintf(stderr, "Cartucho: not enough memory for the RTC\n");
		return 0;
	}
	return 1;
}

int load_rom (Cartucho *cart, const char *filename)
{
	cart->rom_bank = 1;
	cart->bank1 = 1;

	if (!read_rom(cart, filename)) return 0;
	uint32_t header_base = find_header_base(cart);
	parse_cart_header(cart, header_base);

	get_cart_ram_size(cart, header_base);
	if (!cart_alloc_ram(cart)) return 0;

	cart->mbc30 = is_mbc30(cart);
	select_mbc_fx(cart);

	if (!init_special_mbcs(cart)) return 0;

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

static int read_int64 (FILE *f, int64_t *v)
{
	*v = 0;
	for (int i = 0; i < 8; i++) {
		int a = fgetc(f);
		if (a == EOF) return 0;
		*v |= (int64_t)((uint64_t)(uint8_t)a) << (i * 8);
	}
	return 1;
}

static int write_int64 (FILE *f, int64_t v)
{
	for (int i = 0; i < 8; i++) {
		uint8_t a = (v >> (i * 8)) & 0xFF;
		if (fputc(a, f) == EOF) return 0;
	}
	return 1;
}

static void load_rtc_data (RTC *rtc, uint8_t *buf, int64_t base)
{
	rtc->s = buf[0] & 0x3F;
	rtc->m = buf[1] & 0x3F;
	rtc->h = buf[2] & 0x1F;
	rtc->d = ((uint16_t)(buf[4] & 0x01) << 8) | buf[3];
	rtc->halt = (buf[4] & 0x40) != 0;
	rtc->carry = (buf[4] & 0x80) != 0;
	rtc->base = (time_t) base;

	rtc->s_l = rtc->s;
	rtc->m_l = rtc->m;
	rtc->h_l = rtc->h;
	rtc->d_l = rtc->d;
	rtc->halt_l = rtc->halt;
	rtc->carry_l = rtc->carry;
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
		size_t a = fread(cart->ram, 1, cart->ram_size, f);
		if (a != cart->ram_size) {
			fprintf(stderr, "Could not load saved game %s\n", path);
			fclose(f);
			return 0;
		}
	}

	if (cart->has_rtc) {
		uint8_t buf[5];
		int64_t base;

		if (fread(buf, 1, 5, f) == 5 && read_int64(f, &base)) {
			load_rtc_data((RTC *)cart->state, buf, base);
		} else {
			printf("No RTC data in %s, starting clock fresh\n", path);
		}
	}

	fclose(f);
	printf("Loaded game: %s\n", path);
	return 1;
}

static int write_sav_file (const char *savefile, const uint8_t *ram,
			uint32_t ram_size, const RTC *rtc)
{
	char tmp_path[520];
	int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", savefile);
	if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
		fprintf(stderr, "Save path too long: %s\n", savefile);
		return 0;
	}

	FILE *f = fopen(tmp_path, "wb");
	if (!f) {
		fprintf(stderr, "Could not save game %s\n", savefile);
		return 0;
	}

	if (ram && ram_size > 0)
		if (fwrite(ram, 1, ram_size, f) != ram_size) goto fail;

	if (rtc) {
		uint8_t dh = ((rtc->d >> 8) & 0x01)
			| (rtc->halt  ? 0x40 : 0)
			| (rtc->carry ? 0x80 : 0);
		uint8_t buf[5] = { rtc->s, rtc->m, rtc->h,
				(uint8_t)(rtc->d & 0xFF), dh };
		int64_t base = (int64_t) rtc->base;

		if (fwrite(buf, 1, 5, f) != 5) goto fail;
		if (!write_int64(f, base)) goto fail;
	}

	if (fflush(f) != 0) goto fail;
	if (fclose(f) != 0) {
		fprintf(stderr, "Could not save game. File %s was corrupted\n", savefile);
		remove(tmp_path);
		return 0;
	}
	if (rename(tmp_path, savefile) != 0) {
		fprintf(stderr, "Could not rename temp save to %s\n", savefile);
		remove(tmp_path);
		return 0;
	}
	return 1;

fail:
	fprintf(stderr, "Could not save game. File %s was corrupted\n", savefile);
	fclose(f);
	remove(tmp_path);
	return 0;
}

int save_sram (Cartucho *cart, const char *romfile)
{
	if (!cart->battery) return 0;
	if ((!cart->ram || cart->ram_size == 0) && !cart->has_rtc) return 0;

	char savefile[512];
	make_sav_path(romfile, savefile, sizeof(savefile));

	int save_success = write_sav_file(savefile, cart->ram, cart->ram_size,
					cart->has_rtc ? cart->state : NULL);

	if (save_success) {
		printf("Saved game: %s\n", savefile);
		return 1;
	}
	return 0;
}

void save_sram_snapshot (Cartucho *cart, const char *romfile,
			 const uint8_t *ram_snap, uint32_t ram_size,
			 const RTC *rtc_snap)
{
	if (!cart->battery) return;
	if ((!ram_snap || ram_size == 0) && !cart->has_rtc) return;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));
	write_sav_file(path, ram_snap, ram_size,
			cart->has_rtc ? rtc_snap : NULL);
}

void free_cart (Cartucho *cart)
{
	if (cart->mbc_type == MBC6) mbc6_free(cart);
	if (cart->mbc_type == MBC7) mbc7_free(cart);
	if (cart->mbc_type == MMM01) mmm01_free(cart);
	if (cart->has_rtc) rtc_free(cart);
	free(cart->rom);
	free(cart->ram);
}
