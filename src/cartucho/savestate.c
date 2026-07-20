#include "cartucho/savestate.h"
#include "gb.h"

#include <string.h>
#include <stddef.h>
#include <pthread.h>

static void io_buf (SaveState *s, void *p, size_t n)
{
	if (!s->ok || n == 0) return;

	size_t r;
	if (s->saving) r = fwrite(p, 1, n, s->f);
	else r = fread(p, 1, n, s->f);

	if (r != n) s->ok = 0;
}

static void io_num (SaveState *s, void *v) {
	io_buf(s, v, sizeof(*v));
}

static void io_cpu (SaveState *s, CPU *cpu)
{
	io_buf(s, cpu, offsetof(CPU, instr_stack));
	if (!s->saving) {
		cpu->instr_head = 0;
		cpu->instr_tail = 0;
	}
}

static void io_ppu (SaveState *s, PPU *ppu)
{
	io_buf(s, ppu, offsetof(PPU, bus));
	io_buf(s, (uint8_t *)ppu + offsetof(PPU, palette),
	       sizeof(PPU) - offsetof(PPU, palette));
}

static void io_mbc6 (SaveState *s, MBC6State *m)
{
	io_num(s, &m->rom_bank_a);
	io_num(s, &m->rom_bank_b);
	io_num(s, &m->flash_sel_a);
	io_num(s, &m->flash_sel_b);
	io_num(s, &m->ram_bank_a);
	io_num(s, &m->ram_bank_b);
	io_num(s, &m->ram_enabled);
	io_num(s, &m->flash_enabled);
	io_num(s, &m->flash_write_enabled);
	io_num(s, &m->flash.sector0_locked);
	io_num(s, &m->flash.write_enable);
	io_num(s, &m->flash.program_addr);
	io_buf(s, m->flash.program_buf, sizeof(m->flash.program_buf));
	io_num(s, &m->flash.program_len);
	io_num(s, &m->flash.program_hidden);
	io_num(s, &m->flash.state);
	io_num(s, &m->flash.status);
	io_buf(s, m->flash.hidden, sizeof(m->flash.hidden));
	if (m->flash.data) io_buf(s, m->flash.data, MBC6_FLASH_SIZE);
}

static void io_mbc7 (SaveState *s, MBC7State *m)
{
	io_num(s, &m->ram_enable1);
	io_num(s, &m->ram_enable2);
	io_num(s, &m->latch_x);
	io_num(s, &m->latch_y);
	io_num(s, &m->latched);
	io_num(s, &m->eeprom_enabled);
	io_num(s, &m->cs);
	io_num(s, &m->clk);
	io_num(s, &m->di);
	io_num(s, &m->do_bit);
	io_num(s, &m->start_seen);
	io_num(s, &m->shift_reg);
	io_num(s, &m->bit_count);
	io_num(s, &m->op_addr);
	io_num(s, &m->write_all);
	io_num(s, &m->ee_state);
}

static void io_cart (SaveState *s, GB *gb)
{
	Cartucho *cart = &gb->memory.cart;

	io_num(s, &cart->mbc_mode);
	io_num(s, &cart->rom_bank);
	io_num(s, &cart->ram_bank);
	io_num(s, &cart->ram_enabled);
	io_num(s, &cart->bank1);
	io_num(s, &cart->bank2);
	io_num(s, &cart->rumble_on);
 
	pthread_mutex_lock(&gb->save.lock);
	if (cart->has_rtc) io_buf(s, (RTC *)cart->state, sizeof(RTC));
	if (cart->mbc_type == MBC6) io_mbc6(s, (MBC6State *)cart->state);
	if (cart->mbc_type == MBC7) io_mbc7(s, (MBC7State *)cart->state);
	uint32_t ram_size = cart->ram_size;
	io_num(s, &ram_size);
	pthread_mutex_unlock(&gb->save.lock);

	if (!s->saving && s->ok && ram_size != cart->ram_size) {
		fprintf(stderr, "SaveState: Cart RAM size doesn't match (state=%u, rom=%u); ¿wrong ROM?\n",
				ram_size, cart->ram_size);
		s->ok = 0;
		return;
	}
	if (cart->ram && cart->ram_size > 0)
		io_buf(s, cart->ram, cart->ram_size);
}

static void io_memory (SaveState *s, GB *gb)
{
	Memory *mem = &gb->memory;
	io_cart(s, gb);
	io_buf(s, mem->bios, sizeof(mem->bios));
	io_buf(s, mem->vram, sizeof(mem->vram));
	io_buf(s, mem->wram, sizeof(mem->wram));
	io_buf(s, mem->oam,  sizeof(mem->oam));
	io_buf(s, mem->hram, sizeof(mem->hram));
}
 
static void io_serial (SaveState *s, Serial *serial)
{
	io_num(s, &serial->SB);
	io_num(s, &serial->SC);
	io_num(s, &serial->transfer_active);
	io_num(s, &serial->clock);
	io_num(s, &serial->recived);
	io_num(s, &serial->sent);
	io_num(s, &serial->shifted);
	io_num(s, &serial->buff);
}

static int state_io (GB *gb, FILE *f, int saving)
{
	SaveState ss;
	ss.f = f;
	ss.saving = saving;
	ss.ok = 1;
 
	io_cpu(&ss, &gb->cpu);
	io_ppu(&ss, &gb->ppu);
	io_buf(&ss, &gb->apu, sizeof(APU));
	io_buf(&ss, &gb->interrupts, sizeof(Interrupts));
	io_buf(&ss, &gb->timer, sizeof(Timer));
	io_buf(&ss, &gb->joypad, sizeof(Joypad));
	io_serial(&ss, &gb->serial);
	io_buf(&ss, &gb->dma, sizeof(DMA));
	io_num(&ss, &gb->boot_rom_enabled);
	io_num(&ss, &gb->boot_rom_disable_pending);
	io_num(&ss, &gb->clock);
	io_memory(&ss, gb);
 
	return ss.ok;
}

static void state_path (const char *romfile, char *out, size_t outsize, int save_num)
{
	size_t len = strlen(romfile);
	const char *dot = strrchr(romfile, '.');
	const char *slash = strrchr(romfile, '/');
 
	size_t base_len = len;
	if (dot && (!slash || dot > slash))
		base_len = (size_t)(dot - romfile);
 
	char extension[6];
	int ext_len = snprintf(extension, sizeof(extension), ".ss%d", save_num);
	if (ext_len < 0) return;

	if (base_len + (size_t)ext_len + 1 > outsize) return;
	memcpy(out, romfile, base_len);
	memcpy(out + base_len, extension, (size_t)ext_len + 1);
}
 
int can_save_state (GB *gb) {
	return gb->cpu.instr_head >= gb->cpu.instr_tail;
}

int save_state (GB *gb)
{
	char path[600];
	char tmp_path[608];
	state_path(gb->romfile, path, sizeof(path), gb->state_num);
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
 
	FILE *f = fopen(tmp_path, "wb");
	if (!f) {
		fprintf(stderr, "SaveState: could not open %s for writing\n", tmp_path);
		return 0;
	}
 
	StateHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic = SAVESTATE_MAGIC;
	hdr.rom_size = gb->memory.cart.rom_size;
	memcpy(hdr.title, gb->memory.cart.title, sizeof(hdr.title));
 
	int ok = fwrite(&hdr, sizeof(hdr), 1, f) == 1;
	if (ok) ok = state_io(gb, f, 1);
	if (ok && fflush(f) != 0) ok = 0;
	fclose(f);
 
	if (!ok) {
		remove(tmp_path);
		fprintf(stderr, "SaveState: failed to save the state\n");
		return 0;
	}
 
	if (rename(tmp_path, path) != 0) {
		fprintf(stderr, "SaveState: could not finish %s\n", path);
		remove(tmp_path);
		return 0;
	}
 
	printf("State saved: %s\n", path);
	return 1;
}
 
int load_state (GB *gb)
{
	char path[600];
	state_path(gb->romfile, path, sizeof(path), gb->state_num);
 
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "SaveState: file %s not exists\n", path);
		return 0;
	}
 
	StateHeader hdr;
	int ok = fread(&hdr, sizeof(hdr), 1, f) == 1;
 
	if (ok && hdr.magic != SAVESTATE_MAGIC) {
		fprintf(stderr, "SaveState: %s is not a valid savestate file\n", path);
		ok = 0;
	}
	if (ok && hdr.rom_size != gb->memory.cart.rom_size) {
		fprintf(stderr, "SaveState: %s saved with a different ROM (size doesn't match)\n", path);
		ok = 0;
	}
	if (ok && memcmp(hdr.title, gb->memory.cart.title, sizeof(hdr.title)) != 0) {
		fprintf(stderr, "SaveState: %s seems to be a savestate file from another game\n", path);
		ok = 0;
	}
 
	if (ok) ok = state_io(gb, f, 0);
	fclose(f);
 
	if (!ok) {
		fprintf(stderr, "SaveState: failed to load the state\n");
		return 0;
	}
 
	pthread_mutex_lock(&gb->save.lock);
	gb->memory.cart.save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
 
	printf("State loaded: %s\n", path);
	return 1;
}
