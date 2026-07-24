#include "gb.h"

#include <stdio.h>
#include <string.h>

static int load_bios (GB *gb, const char *filename)
{
	if (!filename) return 0;

	FILE *f = fopen(filename, "rb");
	if (!f) return 0;

	size_t n = fread(gb->memory.bios, 1, 0x100, f);
	fclose(f);

	return n == 0x100;
}

void init_regs (GB *gb)
{
	memset(gb->memory.vram, 0, sizeof(Memory) - offsetof(Memory, vram));

	memset(&gb->ppu, 0, sizeof(PPU));
	init_ppu(&gb->ppu);
	gb->ppu.palette = gb->cfg.palette;
	gb->ppu.lcd_was_off = 1;

	memset(&gb->apu, 0, sizeof(APU));
	init_apu(&gb->apu);
	gb->apu.sample_rate = gb->audio.sample_rate;

	memset(&gb->cpu, 0, sizeof(CPU));
	memset(&gb->dma, 0, sizeof(DMA));
	memset(&gb->timer, 0, sizeof(Timer));
	memset(&gb->serial, 0, sizeof(Serial));
	memset(&gb->interrupts, 0, sizeof(Interrupts));

	if (gb->link.active) gb->serial.link = &gb->link;
	if (gb->printer.enabled) gb->serial.printer = &gb->printer;

	if (!gb->hay_bios) {
		init_ppu_reg(&gb->ppu);
		init_apu_reg(&gb->apu);
		init_cpu(&gb->cpu);
		gb->timer.div = 0xABCC;
		gb->serial.SC = 0x7E;
		gb->timer.tac = 0xF8;
		gb->joypad.joyp = 0xCF;
		gb->boot_rom_enabled = 0;
	} else {
		gb->cpu.pc = 0;
		gb->boot_rom_enabled = 1;
	}

	init_bus(&gb->bus, gb);
	gb->on = 1;
}

static int init_core (GB *gb, const char *romfile, const char *biosfile)
{
	memset(gb, 0, sizeof(GB));
	gb->romfile = romfile;

	pthread_mutex_init(&gb->save.lock, NULL);
	pthread_cond_init(&gb->save.cond, NULL);
	atomic_store(&gb->save.request, 0);
	atomic_store(&gb->save.thread_run, 0);

	init_ppu(&gb->ppu);
	init_apu(&gb->apu);
	init_opcodes(&gb->opcodes);
	init_printer(&gb->printer);

	init_config_defaults(&gb->cfg);

	if (!load_bios(gb, biosfile)) {

		init_ppu_reg(&gb->ppu);
		init_apu_reg(&gb->apu);
		init_cpu(&gb->cpu);
		gb->timer.div = 0xABCC;
		gb->serial.SC = 0x7E;
		gb->timer.tac = 0xF8;
		gb->joypad.joyp = 0xCF;
		gb->boot_rom_enabled = 0;
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

	return 0;
}

void init (GB *gb, const char *romfile, const char *biosfile)
{
	if (init_core(gb, romfile, biosfile)) {
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

void init_test (GB *gb, const char *romfile)
{
	if (init_core(gb, romfile, NULL)) {
		gb->running = 0;
		return;
	}

	gb->running = 1;
	gb->on = 1;
}

void cleanup_core (GB *gb, const char *romfile)
{
	pthread_mutex_lock(&gb->save.lock);
	save_sram(&gb->memory.cart, romfile);
	pthread_mutex_unlock(&gb->save.lock);

	free_cart(&gb->memory.cart);
	pthread_cond_destroy(&gb->save.cond);
	pthread_mutex_destroy(&gb->save.lock);
}

void cleanup (GB *gb, const char *romfile)
{
	stop_save_thread(gb);
	link_close(&gb->link);
	frontend_shutdown(gb);
	cleanup_core(gb, romfile);
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

static void stop_step (GB *gb) {
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

	if (gb->boot_rom_disable_pending && gb->cpu.pc >= 0x100) {
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
