#include "frontend/config_ui.h"
#include "frontend/config.h"
#include "frontend/gamepad.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define REMAP_WIN_W 820
#define REMAP_WIN_H 640
#define ROW_H 28
#define ROW_START_Y 96
#define FOOTER_H 78

typedef enum {
	PAGE_CONTROLS = 0,
	PAGE_SETTINGS,
	PAGE_PATHS,
	PAGE_COUNT
} ConfigPage;

typedef enum {
	CAP_NONE = 0,
	CAP_KEY,
	CAP_TEXT,
	CAP_PAD
} CaptureMode;

typedef struct ConfigUI {
	SDL_Window *window;
	SDL_Renderer *renderer;
	TTF_Font *font;

	Config cfg;
	Keymap default_keymap_cache;
	Padmap default_padmap_cache;

	Gamepad *pad;

	ConfigPage page;
	int cursor[PAGE_COUNT];
	int scroll[PAGE_COUNT];

	char status_msg[64];
	int status_ttl;

	char edit_buf[256];
	int edit_len;

	CaptureMode capturing;
	int running;
} ConfigUI;

static int init_window (ConfigUI *ui)
{
	ui->window = SDL_CreateWindow(
		"R2-Boy - Configuration",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		REMAP_WIN_W, REMAP_WIN_H,
		SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
	);
	if (!ui->window) {
		fprintf(stderr, "Config: SDL_CreateWindow error: %s\n", SDL_GetError());
		return 0;
	}

	ui->renderer = SDL_CreateRenderer(ui->window, -1, SDL_RENDERER_ACCELERATED);
	if (!ui->renderer)
		ui->renderer = SDL_CreateRenderer(ui->window, -1, SDL_RENDERER_SOFTWARE);
	if (!ui->renderer) {
		fprintf(stderr, "Config: SDL_CreateRenderer error: %s\n", SDL_GetError());
		SDL_DestroyWindow(ui->window);
		ui->window = NULL;
		return 0;
	}
	return 1;
}

static void init_font (ConfigUI *ui)
{
	static const char *candidates[] = {
		/* Linux */
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
		"/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
		"/usr/share/fonts/truetype/freefont/FreeSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/local/share/fonts/dejavu/DejaVuSans.ttf",
		/* macOS (system + user fonts) */
		"/System/Library/Fonts/Helvetica.ttc",
		"/System/Library/Fonts/SFNSMono.ttf",
		"/System/Library/Fonts/Supplemental/Arial.ttf",
		"/Library/Fonts/Arial.ttf",
		NULL, NULL
	};
	const char *home = getenv("HOME");
	if (home) {
		char buf[512];
		snprintf(buf, sizeof(buf), "%s/Library/Fonts/Arial.ttf", home);
		ui->font = TTF_OpenFont(buf, 18);
	}

	for (int i = 0; candidates[i]; i++) {
		ui->font = TTF_OpenFont(candidates[i], 18);
		if (ui->font) return;
	}
}

static int config_open_window (ConfigUI *ui)
{
	if (!init_window(ui)) return 0;
	init_font(ui);

	if (!ui->font) {
		fprintf(stderr, "Config: could not load a font (tried the bundled font and common system fonts)\n");
		SDL_DestroyRenderer(ui->renderer);
		SDL_DestroyWindow(ui->window);
		ui->renderer = NULL;
		ui->window = NULL;
		return 0;
	}

	return 1;
}

static void config_close_window (ConfigUI *ui)
{
	if (ui->font) { TTF_CloseFont(ui->font); ui->font = NULL; }
	if (ui->renderer) { SDL_DestroyRenderer(ui->renderer); ui->renderer = NULL; }
	if (ui->window) { SDL_DestroyWindow(ui->window); ui->window = NULL; }
}

static void draw_text (ConfigUI *ui, const char *text, int x, int y, SDL_Color color)
{
	if (!text || !*text) return;

	SDL_Surface *surf = TTF_RenderUTF8_Blended(ui->font, text, color);
	if (!surf) return;

	SDL_Texture *tex = SDL_CreateTextureFromSurface(ui->renderer, surf);
	SDL_Rect dst = { x, y, surf->w, surf->h };
	SDL_FreeSurface(surf);

	if (tex) {
		SDL_RenderCopy(ui->renderer, tex, NULL, &dst);
		SDL_DestroyTexture(tex);
	}
}

static void config_clamp_scroll (ConfigUI *ui)
{
	int vis = (REMAP_WIN_H - ROW_START_Y - FOOTER_H) / ROW_H;
	ConfigPage p = ui->page;

	if (ui->cursor[p] < ui->scroll[p])
		ui->scroll[p] = ui->cursor[p];
	if (ui->cursor[p] >= ui->scroll[p] + vis)
		ui->scroll[p] = ui->cursor[p] - vis + 1;
	if (ui->scroll[p] < 0)
		ui->scroll[p] = 0;
}

static void config_draw_header (ConfigUI *ui)
{
	SDL_Color title = { 255, 255, 255, 255 };
	SDL_Color dim = { 150, 150, 160, 255 };
	SDL_Color active = { 120, 190, 255, 255 };

	draw_text(ui, "R2-Boy - Controls & Settings", 20, 14, title);

	if (ui->pad) {
		char stat[64];
		snprintf(stat, sizeof(stat), "Gamepad: %s",
			ui->pad->connected ? "connected" : "not detected");
		draw_text(ui, stat, REMAP_WIN_W - 260, 16, dim);
	}

	draw_text(ui, "Controls", 20, 50,
		(ui->page == PAGE_CONTROLS) ? active : dim);
	draw_text(ui, "Settings", 220, 50,
		(ui->page == PAGE_SETTINGS) ? active : dim);
	draw_text(ui, "Paths", 420, 50,
		(ui->page == PAGE_PATHS) ? active : dim);

	if (ui->page == PAGE_CONTROLS) {
		draw_text(ui, "Action", 20, 78, dim);
		draw_text(ui, "Keyboard", 320, 78, dim);
		draw_text(ui, "Gamepad", 540, 78, dim);
	} else if (ui->page == PAGE_SETTINGS) {
		draw_text(ui, "Setting", 20, 78, dim);
		draw_text(ui, "Value", 480, 78, dim);
	} else {
		draw_text(ui, "Path", 20, 78, dim);
		draw_text(ui, "Value", 260, 78, dim);
	}
}

static void config_draw_controls (ConfigUI *ui)
{
	SDL_Color normal = { 220, 220, 225, 255 };
	SDL_Color sel_fg = { 15, 15, 20, 255 };
	SDL_Color highlight = { 90, 150, 230, 255 };

	int vis = (REMAP_WIN_H - ROW_START_Y - FOOTER_H) / ROW_H;
	int first = ui->scroll[PAGE_CONTROLS];
	int last = first + vis;
	if (last > ACT_COUNT) last = ACT_COUNT;

	for (int i = first; i < last; i++) {
		int y = ROW_START_Y + (i - first) * ROW_H;
		int selected = (i == ui->cursor[PAGE_CONTROLS]);

		if (selected) {
			SDL_Rect r = { 12, y - 4, REMAP_WIN_W - 24, ROW_H - 2 };
			SDL_SetRenderDrawColor(ui->renderer, highlight.r, highlight.g, highlight.b, 255);
			SDL_RenderFillRect(ui->renderer, &r);
		}
		SDL_Color fg = selected ? sel_fg : normal;

		draw_text(ui, ACTIONS[i].label, 20, y, fg);

		char kbuf[64];
		if (selected && ui->capturing == CAP_KEY)
			snprintf(kbuf, sizeof(kbuf), "Press a key... (Esc cancels)");
		else
			write_keybind_token(kb_binding(&ui->cfg.keymap, (Action) i), kbuf, sizeof(kbuf));
		draw_text(ui, kbuf, 320, y, fg);

		char pbuf[48];
		if (selected && ui->capturing == CAP_PAD)
			snprintf(pbuf, sizeof(pbuf), "Press a button... (Esc cancels)");
		else
			snprintf(pbuf, sizeof(pbuf), "%s",
				pad_button_name(pad_binding(&ui->cfg.padmap, (Action) i)));
		draw_text(ui, pbuf, 540, y, fg);
	}
}

static void config_draw_settings (ConfigUI *ui)
{
	SDL_Color normal = { 220, 220, 225, 255 };
	SDL_Color sel_fg = { 15, 15, 20, 255 };
	SDL_Color highlight = { 90, 150, 230, 255 };

	int vis = (REMAP_WIN_H - ROW_START_Y - FOOTER_H) / ROW_H;
	int first = ui->scroll[PAGE_SETTINGS];
	int last = first + vis;
	if (last > SET_COUNT) last = SET_COUNT;

	for (int i = first; i < last; i++) {
		int y = ROW_START_Y + (i - first) * ROW_H;
		int selected = (i == ui->cursor[PAGE_SETTINGS]);

		if (selected) {
			SDL_Rect r = { 12, y - 4, REMAP_WIN_W - 24, ROW_H - 2 };
			SDL_SetRenderDrawColor(ui->renderer, highlight.r, highlight.g, highlight.b, 255);
			SDL_RenderFillRect(ui->renderer, &r);
		}
		SDL_Color fg = selected ? sel_fg : normal;

		draw_text(ui, SETTINGS[i].label, 20, y, fg);

		char vbuf[32];
		format_setting_value(&ui->cfg, (SettingId) i, vbuf, sizeof(vbuf));
		draw_text(ui, vbuf, 480, y, fg);
	}
}

static void config_draw_paths (ConfigUI *ui)
{
	SDL_Color normal = { 220, 220, 225, 255 };
	SDL_Color sel_fg = { 15, 15, 20, 255 };
	SDL_Color highlight = { 90, 150, 230, 255 };

	const char *labels[2] = {"DMG BIOS", "CGB BIOS"};

	for (int i = 0; i < 2; i++) {
		int y = ROW_START_Y + i * ROW_H;
		int selected = (i == ui->cursor[PAGE_PATHS]);

		if (selected) {
			SDL_Rect r = { 12, y - 4, REMAP_WIN_W - 24, ROW_H - 2 };
			SDL_SetRenderDrawColor(ui->renderer, highlight.r, highlight.g, highlight.b, 255);
			SDL_RenderFillRect(ui->renderer, &r);
		}
		SDL_Color fg = selected ? sel_fg : normal;

		draw_text(ui, labels[i], 20, y, fg);

		char vbuf[258];
		if (selected && ui->capturing == CAP_TEXT)
			snprintf(vbuf, sizeof(vbuf), "%s_", ui->edit_buf);
		else
			snprintf(vbuf, sizeof(vbuf), "%s", i ? ui->cfg.bios_cgb_path : ui->cfg.bios_dmg_path);
		draw_text(ui, vbuf, 260, y, fg);
	}

}

static void config_draw_footer (ConfigUI *ui)
{
	SDL_Color dim = { 170, 170, 180, 255 };

	SDL_SetRenderDrawColor(ui->renderer, 38, 38, 44, 255);
	SDL_Rect bar = { 0, REMAP_WIN_H - FOOTER_H, REMAP_WIN_W, FOOTER_H };
	SDL_RenderFillRect(ui->renderer, &bar);

	int y = REMAP_WIN_H - FOOTER_H + 10;

	if (ui->page == PAGE_CONTROLS) {
		draw_text(ui,
			"Up/Down: select   Enter: bind key   Tab: bind gamepad   D: default",
			20, y, dim);
	} else if (ui->page == PAGE_SETTINGS) {
		draw_text(ui,
			"Up/Down: select   Left/Right or Enter: change value   D: default",
			20, y, dim);
	} else {
		draw_text(ui,
			"Up/Down: select   Enter: edit path   D: default",
			20, y, dim);
	}
	draw_text(ui, "R: reset section   Q/E: switch page   S: save & exit   Esc: cancel / exit",
		20, y + 24, dim);
}

static void config_render (ConfigUI *ui)
{
	config_clamp_scroll(ui);

	SDL_SetRenderDrawColor(ui->renderer, 22, 22, 26, 255);
	SDL_RenderClear(ui->renderer);

	config_draw_header(ui);

	if (ui->page == PAGE_CONTROLS)
		config_draw_controls(ui);
	else if (ui->page == PAGE_SETTINGS)
		config_draw_settings(ui);
	else
		config_draw_paths(ui);

	config_draw_footer(ui);

	SDL_RenderPresent(ui->renderer);
}

static int is_modifier_scancode (SDL_Scancode sc)
{
	switch (sc)
	{
	case SDL_SCANCODE_LCTRL:	case SDL_SCANCODE_RCTRL:
	case SDL_SCANCODE_LSHIFT:	case SDL_SCANCODE_RSHIFT:
	case SDL_SCANCODE_LALT:		case SDL_SCANCODE_RALT:
	case SDL_SCANCODE_LGUI:		case SDL_SCANCODE_RGUI:
		return 1;
	default:
		return 0;
	}
}

static void config_handle_key_capture (ConfigUI *ui, SDL_Event *e)
{
	if (e->type == SDL_QUIT) {
		ui->running = 0;
		return;
	}
	if (e->type != SDL_KEYDOWN) return;

	SDL_Scancode sc = e->key.keysym.scancode;
	if (sc == SDL_SCANCODE_ESCAPE) {
		ui->capturing = CAP_NONE;
		return;
	}
	if (is_modifier_scancode(sc)) return;

	Keybind kb = { sc, (uint16_t)(e->key.keysym.mod & KBMOD_ANY) };
	Action target = (Action) ui->cursor[PAGE_CONTROLS];

	int conflict = find_conflicting_kb_action(&ui->cfg.keymap, kb, target);
	if (conflict >= 0) {
		Keybind none = { SDL_SCANCODE_UNKNOWN, 0 };
		set_kb_binding(&ui->cfg.keymap, (Action)conflict, none);
		snprintf(ui->status_msg, sizeof(ui->status_msg),
				"Removed from %s (was bound there)", ACTIONS[conflict].label);
		ui->status_ttl = 120;
	}

	set_kb_binding(&ui->cfg.keymap, target, kb);
	ui->capturing = CAP_NONE;
}

static void config_handle_text_capture (ConfigUI *ui, SDL_Event *e)
{
	if (e->type == SDL_QUIT) {
		ui->running = 0;
		return;
	}

	if (e->type == SDL_TEXTINPUT) {
		size_t add_len = strlen(e->text.text);
		if (ui->edit_len + add_len < sizeof(ui->edit_buf)) {
			memcpy(ui->edit_buf + ui->edit_len, e->text.text, add_len);
			ui->edit_len += (int)add_len;
			ui->edit_buf[ui->edit_len] = 0;
		}
		return;
	}

	if (e->type != SDL_KEYDOWN) return;

	SDL_Scancode sc = e->key.keysym.scancode;
	if (sc == SDL_SCANCODE_ESCAPE) {
		ui->capturing = CAP_NONE;
		SDL_StopTextInput();
		return;
	}
	if (sc == SDL_SCANCODE_BACKSPACE && ui->edit_len > 0) {
		ui->edit_buf[--ui->edit_len] = 0;
		return;
	}
	if (sc == SDL_SCANCODE_RETURN) {
		char *field = ui->cursor[PAGE_PATHS] ? ui->cfg.bios_cgb_path : ui->cfg.bios_dmg_path;
		snprintf(field, 256, "%s", ui->edit_buf);
		ui->capturing = CAP_NONE;
		SDL_StopTextInput();
	}
}

static void config_handle_pad_capture (ConfigUI *ui, SDL_Event *e)
{
	if (e->type == SDL_QUIT) {
		ui->running = 0;
		return;
	}

	if (e->type == SDL_KEYDOWN && e->key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
		ui->capturing = CAP_NONE;
		return;
	}
	if (e->type != SDL_CONTROLLERBUTTONDOWN) return;

	SDL_GameControllerButton b = (SDL_GameControllerButton) e->cbutton.button;
	Action target = (Action) ui->cursor[PAGE_CONTROLS];

	int conflict = find_conflicting_pad_action(&ui->cfg.padmap, b, target);
	if (conflict >= 0) {
		set_pad_binding(&ui->cfg.padmap, (Action) conflict, SDL_CONTROLLER_BUTTON_INVALID);
		snprintf(ui->status_msg, sizeof(ui->status_msg),
				"Removed from %s (was bound there)", ACTIONS[conflict].label);
		ui->status_ttl = 120;
	}

	set_pad_binding(&ui->cfg.padmap, target, b);
	ui->capturing = CAP_NONE;
}

static void config_controls_key (ConfigUI *ui, SDL_Scancode sc)
{
	Action a = (Action) ui->cursor[PAGE_CONTROLS];

	switch (sc)
	{
	case SDL_SCANCODE_RETURN:
		ui->capturing = CAP_KEY;
		break;
	case SDL_SCANCODE_TAB:
		ui->capturing = CAP_PAD;
		break;
	case SDL_SCANCODE_D:
		set_kb_binding(&ui->cfg.keymap, a, kb_binding(&ui->default_keymap_cache, a));
		set_pad_binding(&ui->cfg.padmap, a, pad_binding(&ui->default_padmap_cache, a));
		break;
	default:
		break;
	}
}

static void config_settings_key (ConfigUI *ui, SDL_Scancode sc)
{
	SettingId id = (SettingId) ui->cursor[PAGE_SETTINGS];
	const SettingMeta *m = &SETTINGS[id];
	int v = get_setting_value(&ui->cfg, id);

	switch (sc)
	{
	case SDL_SCANCODE_LEFT: {
		int nv = v - m->step;
		if (m->kind == SETTING_ENUM && nv < m->min) nv = m->max;
		set_setting_value(&ui->cfg, id, nv);
		break;
	}
	case SDL_SCANCODE_RIGHT: {
		int nv = v + m->step;
		if (m->kind == SETTING_ENUM && nv > m->max) nv = m->min;
		set_setting_value(&ui->cfg, id, nv);
		break;
	}
	case SDL_SCANCODE_RETURN: {
		if (m->kind == SETTING_BOOL) {
			set_setting_value(&ui->cfg, id, !v);
		} else {
			int nv = v + m->step;
			if (nv > m->max) nv = m->min;
			set_setting_value(&ui->cfg, id, nv);
		}
		break;
	}
	case SDL_SCANCODE_D:
		set_setting_value(&ui->cfg, id, default_setting_value(id));
		break;
	default:
		break;
	}
}

static void config_paths_key (ConfigUI *ui, SDL_Scancode sc)
{
	int row = ui->cursor[PAGE_PATHS];

	switch (sc)
	{
	case SDL_SCANCODE_RETURN: {
		char *field = row ? ui->cfg.bios_cgb_path : ui->cfg.bios_dmg_path;
		snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%s", field);
		ui->edit_len = (int)strlen(ui->edit_buf);
		ui->capturing = CAP_TEXT;
		SDL_StartTextInput();
		break;
	}
	case SDL_SCANCODE_D:
		char *field = row ? ui->cfg.bios_cgb_path : ui->cfg.bios_dmg_path;
		snprintf(field, 256, "%s", row ? "roms/cgb_bios.bin" : "roms/bios.bin");
		break;
	default:
		break;
	}
}

static void config_reset_section (ConfigUI *ui)
{
	if (ui->page == PAGE_CONTROLS) {
		for (int i = 0; i < ACT_COUNT; i++) {
			set_kb_binding(&ui->cfg.keymap, (Action) i,
				kb_binding(&ui->default_keymap_cache, (Action) i));
			set_pad_binding(&ui->cfg.padmap, (Action) i,
				pad_binding(&ui->default_padmap_cache, (Action) i));
		}
	} else if (ui->page == PAGE_SETTINGS) {
		for (int i = 0; i < SET_COUNT; i++)
			set_setting_value(&ui->cfg, (SettingId) i, default_setting_value((SettingId) i));
	} else {
		char *field = ui->cfg.bios_dmg_path;
		snprintf(field, 256, "%s", "roms/bios.bin");
		field = ui->cfg.bios_cgb_path;
		snprintf(field, 256, "%s", "roms/cgb_bios.bin");
	}
}

static void config_handle_keydown (ConfigUI *ui, SDL_KeyboardEvent *key)
{
	SDL_Scancode sc = key->keysym.scancode;
	int rows = ACT_COUNT;
	if (ui->page == PAGE_SETTINGS) rows = SET_COUNT;
	else if (ui->page == PAGE_PATHS) rows = 2;

	switch (sc)
	{
	case SDL_SCANCODE_ESCAPE:
		ui->running = 0;
		return;
	case SDL_SCANCODE_S:
		save_config(&ui->cfg);
		ui->running = 0;
		return;
	case SDL_SCANCODE_Q:
		ui->page = (ui->page + PAGE_COUNT - 1) % PAGE_COUNT;
		return;
	case SDL_SCANCODE_E:
		ui->page = (ui->page + 1) % PAGE_COUNT;
		return;
	case SDL_SCANCODE_R:
		config_reset_section(ui);
		return;
	case SDL_SCANCODE_UP:
		if (ui->cursor[ui->page] > 0) ui->cursor[ui->page]--;
		return;
	case SDL_SCANCODE_DOWN:
		if (ui->cursor[ui->page] < rows - 1) ui->cursor[ui->page]++;
		return;
	default:
		break;
	}

	if (ui->page == PAGE_CONTROLS)
		config_controls_key(ui, sc);
	else if (ui->page == PAGE_CONTROLS)
		config_settings_key(ui, sc);
	else
		config_paths_key(ui, sc);
}

static void config_handle_events (ConfigUI *ui, Gamepad *pad)
{
	SDL_Event e;
	while (SDL_PollEvent(&e)) {

		if (e.type == SDL_QUIT) {
			ui->running = 0;
			return;
		}
		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
			ui->running = 0;
			return;
		}
		if (e.type == SDL_CONTROLLERDEVICEADDED) {
			open_gamepad(pad, e.cdevice.which);
			continue;
		}
		if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
			if (pad->connected && e.cdevice.which == pad->instance_id)
				cleanup_gamepad(pad);
			continue;
		}

		if (ui->capturing == CAP_KEY) {
			config_handle_key_capture(ui, &e);
			continue;
		}
		if (ui->capturing == CAP_PAD) {
			config_handle_pad_capture(ui, &e);
			continue;
		}
		if (ui->capturing == CAP_TEXT) {
			config_handle_text_capture(ui, &e);
			continue;
		}

		if (e.type == SDL_KEYDOWN)
			config_handle_keydown(ui, &e.key);
	}
}

void run_config (void)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Config: SDL_Init error: %s\n", SDL_GetError());
		return;
	}
	if (TTF_Init() != 0) {
		fprintf(stderr, "Config: TTF_Init error: %s\n", TTF_GetError());
		SDL_Quit();
		return;
	}

	ConfigUI ui;
	memset(&ui, 0, sizeof(ui));

	init_config_defaults(&ui.cfg);
	load_config(&ui.cfg);

	default_keymap(&ui.default_keymap_cache);
	default_padmap(&ui.default_padmap_cache);

	if (!config_open_window(&ui)) {
		TTF_Quit();
		SDL_Quit();
		return;
	}

	Gamepad pad;
	init_gamepad(&pad);
	ui.pad = &pad;

	ui.running = 1;
	while (ui.running) {
		config_handle_events(&ui, &pad);
		config_render(&ui);
		SDL_Delay(16);
	}

	cleanup_gamepad(&pad);
	config_close_window(&ui);
	TTF_Quit();
	SDL_Quit();
}
