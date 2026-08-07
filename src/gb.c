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
	memset(&gb->hdma, 0, sizeof(HDMA));
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
	uint8_t *bg = gb->memory.bg_palette_ram;
	uint8_t *sp = gb->memory.obj_palette_ram;
	memset(bg, 0xFF, sizeof(gb->memory.bg_palette_ram));
	memset(sp, 0xFF, sizeof(gb->memory.obj_palette_ram));

	uint16_t colors[] = {
		0xFF, 0x7F,
		0xB5, 0x56,
		0x4A, 0x29,
		0x00, 0x00
	};
	for (int i = 0; i < 8; i++) {
		bg[i] = colors[i];
		sp[i] = colors[i];
		sp[8 + i] = colors[i];
	}
}

static void detect_dmg_mode (GB *gb)
{
	if (gb->model != CGB) return;
	uint8_t cgb_flag = gb->memory.cart.rom[find_header_base(&gb->memory.cart) + 0x143];
	gb->memory.key0 = (cgb_flag == 0x80 || cgb_flag == 0xC0) ? 0x00 : 0x04;
}

static void reset_regs (GB *gb)
{
	init_ppu_reg(&gb->ppu);
	init_apu_reg(&gb->apu);
	init_cpu(&gb->cpu, gb->model);
	init_cgb_palette_ram(gb);
	gb->timer.div = (gb->model == CGB) ? 0x2678 : 0xABCC;
	gb->joypad.joyp = (gb->model == CGB) ? 0x30 : 0xCF;
	gb->memory.wram_bank = 1;
	gb->timer.tac = 0xF8;
	gb->serial.SC = 0x7E;
	gb->boot_rom_enabled = 0;
}

static void reset_bios (GB *gb)
{
	if (!gb->hay_bios) {
		reset_regs(gb);
		detect_dmg_mode(gb);
	} else {
		gb->cpu.pc = 0;
		gb->boot_rom_enabled = 1;
	}
}

void reset_gb (GB *gb)
{
	memset(gb->memory.vram, 0, gb->model == CGB ? 0x4000 : 0x2000);
	memset(gb->memory.wram, 0, gb->model == CGB ? 0x8000 : 0x2000);
	memset((uint8_t *)&gb->memory + offsetof(Memory, oam), 0,
			sizeof(Memory) - offsetof(Memory, oam));

	reset_components(gb);

	reset_bios(gb);
	init_bus(&gb->bus, gb);

	reset_rest(gb);
	gb->on = 1;
}

static int alloc_memory_banks (Memory *mem, Model model)
{
	uint16_t vram_size = (model == CGB) ? 0x4000 : 0x2000;
	uint16_t wram_size = (model == CGB) ? 0x8000 : 0x2000;
	uint16_t bios_size = (model == CGB) ? 0x900 : 0x100;

	mem->vram = calloc(1, vram_size);
	mem->wram = calloc(1, wram_size);
	mem->bios = calloc(1, bios_size);

	if (!mem->vram || !mem->wram || !mem->bios) {
		fprintf(stderr, "Not enough memory for the Game Boy Memory\n");
		return 0;
	}
	return 1;
}

static void free_memory_banks (Memory *mem)
{
	free(mem->vram);
	free(mem->wram);
	free(mem->bios);
	mem->vram = NULL;
	mem->wram = NULL;
	mem->bios = NULL;
}

static void check_model_compatibility (GB *gb)
{
	if (!gb->hay_bios)
		detect_dmg_mode(gb);

	uint8_t cgb_flag = gb->memory.cart.rom[find_header_base(&gb->memory.cart) + 0x143];
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

	if (!alloc_memory_banks(&gb->memory, model))
		return 1;

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

void init (GB *gb, const char *romfile, const char *biosfile, Model model, uint8_t win_scale)
{
	if (init_core(gb, romfile, biosfile, model)) {
		gb->running = 0;
		return;
	}

	if (!frontend_init(gb, gb->memory.cart.title, win_scale)) {
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
	free_memory_banks(&gb->memory);
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

static int handle_states (GB *gb)
{
	if (!gb->on) {
		if (gb->ppu.shutdown_pending) {
			ppu_shutdown_step(&gb->ppu);
			gb->clock += TICKS_PER_FRAME;
		}
		return 1;
	}

	if (gb->boot_rom_disable_pending && gb->cpu.pc == 0x100) {
		gb->boot_rom_enabled = 0;
		gb->boot_rom_disable_pending = 0;
	}

	if (gb->cpu.speed_switch_delay > 0) {
		gb->cpu.speed_switch_delay--;
		gb->clock += 4;
		return 1;
	}

	if (gb->cpu.stopped) {
		stop_step(gb);
		return 1;
	}
	return 0;
}

static void handle_cpu_step (GB *gb)
{
	if (!gb->hdma.stall)
		cpu_step(&gb->cpu);

	dma_step(gb);

	uint16_t old_div = gb->timer.div;
	if (timer_step(&gb->timer))
		gb->interrupts.IF |= 0x04;

	if (serial_step(&gb->serial, old_div, gb->timer.div))
		gb->interrupts.IF |= 0x08;
}

void gb_step (GB *gb)
{
	if (handle_states(gb)) return;

	handle_cpu_step(gb);
	if (gb->memory.key1 & 0x80)
		handle_cpu_step(gb);

	if (gb->hdma.stall > 4) gb->hdma.stall -= 4;
	else gb->hdma.stall = 0;

	save_state_step(gb);
	printer_step(&gb->printer);
	if (gb->memory.cart.mbc_type == CAM)
		cam_step(gb);

	ppu_step(&gb->ppu);
	apu_step(&gb->apu);
	gb->clock += 4;
}
