#include "gb.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int load_bios (GB *gb, const char *filename, size_t size)
{
	if (!filename) return 0;
	FILE *f = fopen(filename, "rb");
	if (!f) return 0;
	size_t n = fread(gb->memory.bios, 1, size, f);
	fclose(f);
	return n == size;
}

static void reset_cartucho (Cartucho *c)
{
	if (!c->rom) return;
	c->rom_bank = 1;
	c->ram_bank = 0;
	c->ram_enabled = (c->mbc_type == MBC_NONE);
	c->mbc_mode = 0;
	c->bank1 = 1;
	c->bank2 = 0;
	c->save_needed = 0;
	c->rumble_on = 0;
	c->rumble_on_ticks = 0;
	c->rumble_since = 0;
	if (c->mbc_type == MBC6) mbc6_reset(c);
	if (c->mbc_type == MBC7) mbc7_reset(c);
	if (c->mbc_type == HUC3) huc3_reset(c);
}

static void reset_components (GB *gb)
{
	memset(&gb->ppu, 0, sizeof(PPU));
	init_ppu(&gb->ppu, gb->model);
	gb->ppu.palette = gb->cfg.palette;
	gb->ppu.lcd_was_off = 1;

	memset(&gb->apu, 0, sizeof(APU));
	init_apu(&gb->apu, gb->model);
	gb->apu.sample_rate = gb->audio.sample_rate;

	memset(&gb->cpu, 0, sizeof(CPU));
	memset(&gb->dma, 0, sizeof(DMA));
	memset(&gb->timer, 0, sizeof(Timer));
	memset(&gb->serial, 0, sizeof(Serial));
	memset(&gb->interrupts, 0, sizeof(Interrupts));

	if (gb->link.active) gb->serial.link = &gb->link;
	if (gb->printer.enabled) gb->serial.printer = &gb->printer;
}

static void reset_rest (GB *gb)
{
	gb->clock = 0;
	gb->paused = 0;
	gb->state_save_pending = 0;
	gb->state_load_pending = 0;
	gb->boot_rom_disable_pending = 0;

	reset_cartucho(&gb->memory.cart);

	if (gb->audio.dev) {
		ring_clear(&gb->audio.ring);
		gb->apu.buffer_pos = 0;
	}

	if (gb->link.active) {
		ring_reset(&gb->link.rx);
		ring_reset(&gb->link.tx);
	}
}

static void init_cgb_palette_ram (GB *gb)
{
	if (gb->model != CGB) return;
	memset(gb->memory.bg_palette_ram, 0xFF, sizeof(gb->memory.bg_palette_ram));
	memset(gb->memory.obj_palette_ram, 0xFF, sizeof(gb->memory.obj_palette_ram));
}

static void reset_regs (GB *gb)
{
	init_ppu_reg(&gb->ppu);
	init_apu_reg(&gb->apu);
	init_cpu(&gb->cpu, gb->model);
	init_cgb_palette_ram(gb);
	gb->timer.div = (gb->model == CGB) ? 0x2678 : 0xABCC;
	gb->joypad.joyp = (gb->model == CGB) ? 0x30 : 0xCF;
	gb->memory.wram_bank = (gb->model == CGB) ? 0x07 : 0;
	gb->timer.tac = 0xF8;
	gb->serial.SC = 0x7E;
	gb->boot_rom_enabled = 0;
}

static void reset_bios (GB *gb)
{
	if (!gb->hay_bios) {
		reset_regs(gb);
	} else {
		gb->cpu.pc = 0;
		gb->boot_rom_enabled = 1;
	}
}

void reset_gb (GB *gb)
{
	memset(gb->memory.vram, 0, sizeof(Memory) - offsetof(Memory, vram));
	reset_components(gb);

	reset_bios(gb);
	init_bus(&gb->bus, gb);

	reset_rest(gb);
	gb->on = 1;
}

static void check_model_compatibility (GB *gb)
{
	uint8_t cgb_flag = gb->memory.cart.rom[find_header_base(&gb->memory.cart) + 0x143];
	int cgb_aware = (cgb_flag == 0x80 || cgb_flag == 0xC0);

	if (!gb->hay_bios && gb->model == CGB && !cgb_aware)
		gb->memory.key0 = 0x04;

	if (cgb_flag != 0xC0 || gb->model == CGB) return;
	fprintf(stderr, "WARNING: %s is a Game Boy Color exclusive ROM\n", gb->romfile);
}

static int init_core (GB *gb, const char *romfile, const char *biosfile, Model model)
{
	memset(gb, 0, sizeof(GB));
	gb->romfile = romfile;
	gb->model = model;

	pthread_mutex_init(&gb->save.lock, NULL);
	pthread_cond_init(&gb->save.cond, NULL);
	atomic_store(&gb->save.request, 0);
	atomic_store(&gb->save.thread_run, 0);

	init_ppu(&gb->ppu, model);
	init_apu(&gb->apu, model);
	init_opcodes(&gb->opcodes);
	init_printer(&gb->printer);

	init_config_defaults(&gb->cfg);

	size_t bios_size = (model == DMG) ? 0x100 : 0x900;
	if (!load_bios(gb, biosfile, bios_size)) {

		reset_regs(gb);
		gb->hay_bios = 0;
		if (biosfile != NULL)
			printf("Could not load BOOT ROM %s. Running without BIOS\n", biosfile);
	} else {
		printf("Using BOOT ROM %s\n", biosfile);
		gb->boot_rom_enabled = 1;
		gb->hay_bios = 1;
	}

	init_bus(&gb->bus, gb);

	if (!load_rom(&gb->memory.cart, romfile)) {
		fprintf(stderr, "Failed to load ROM: %s\n", romfile);
		return 1;
	}
	load_sram(&gb->memory.cart, romfile);
	check_model_compatibility(gb);

	return 0;
}

void init (GB *gb, const char *romfile, const char *biosfile, Model model)
{
	if (init_core(gb, romfile, biosfile, model)) {
		gb->running = 0;
		return;
	}

	if (!frontend_init(gb, gb->memory.cart.title)) {
		gb->running = 0;
		return;
	}
	gb->apu.sample_rate = gb->audio.sample_rate;

	gb->running = 1;
	gb->on = 1;
}

void init_test (GB *gb, const char *romfile, Model model)
{
	if (init_core(gb, romfile, NULL, model)) {
		gb->running = 0;
		return;
	}

	gb->running = 1;
	gb->on = 1;
}

void cleanup_core (GB *gb)
{
	pthread_mutex_lock(&gb->save.lock);
	save_sram(&gb->memory.cart, gb->romfile);
	pthread_mutex_unlock(&gb->save.lock);

	free_cart(&gb->memory.cart);
	pthread_cond_destroy(&gb->save.cond);
	pthread_mutex_destroy(&gb->save.lock);
}

void cleanup (GB *gb)
{
	stop_save_thread(gb);
	link_close(&gb->link);
	frontend_shutdown(gb);
	cleanup_core(gb);
}

static void dma_step (GB *gb)
{
	if (gb->dma.delay > 0) {
		gb->dma.delay--;
		if (gb->dma.delay == 0)
			gb->dma.active = 1;

	} else if (gb->dma.active) {
		gb->memory.oam[gb->dma.index] = dma_read_source(gb, gb->dma.src + gb->dma.index);
		gb->dma.index++;
		if (gb->dma.index >= 0xA0)
			gb->dma.active = 0;
	}
}

static void save_state_step (GB *gb)
{
	if (!can_save_state(gb)) return;
	if (gb->state_save_pending) {
		save_state(gb);
		gb->state_save_pending = 0;
	}
	if (gb->state_load_pending) {
		load_state(gb);
		gb->state_load_pending = 0;
	}
}

static void stop_step (GB *gb)
{
	gb->ppu.ready = 0;
	cpu_step(&gb->cpu);
	save_state_step(gb);
	printer_step(&gb->printer);
	if (gb->memory.cart.mbc_type == CAM)
		cam_step(gb);
	gb->clock += 4;
}

void gb_step (GB *gb)
{
	if (!gb->on) {
		if (gb->ppu.shutdown_pending) {
			ppu_shutdown_step(&gb->ppu);
			gb->clock += TICKS_PER_FRAME;
		}
		return;
	}

	if (gb->boot_rom_disable_pending && gb->cpu.pc == 0x100) {
		gb->boot_rom_enabled = 0;
		gb->boot_rom_disable_pending = 0;
	}

	if (gb->cpu.stopped) {
		stop_step(gb);
		return;
	}

	cpu_step(&gb->cpu);
	dma_step(gb);

	save_state_step(gb);
	printer_step(&gb->printer);
	if (gb->memory.cart.mbc_type == CAM)
		cam_step(gb);

	uint16_t old_div = gb->timer.div;
	if (timer_step(&gb->timer))
		gb->interrupts.IF |= 0x04;

	if (serial_step(&gb->serial, old_div, gb->timer.div))
		gb->interrupts.IF |= 0x08;

	ppu_step(&gb->ppu);
	apu_step(&gb->apu);
	gb->clock += 4;
}
