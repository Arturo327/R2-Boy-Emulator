#include "frontend/remap_ui.h"
#include "gb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int is_modifier_scancode (SDL_Scancode sc)
{
	switch (sc)
	{
	case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL:
	case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT:
	case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT:
	case SDL_SCANCODE_LGUI: case SDL_SCANCODE_RGUI:
		return 1;
	default:
		return 0;
	}
}

static TTF_Font *open_font (int size)
{
	static const char *fonts[] = {
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
		"/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
		"/usr/share/fonts/truetype/freefont/FreeSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/local/share/fonts/dejavu/DejaVuSans.ttf",
	};
	int n = sizeof(fonts) / sizeof(fonts[0]);
	for (int i = 0; i < n; i++) {
		TTF_Font *f = TTF_OpenFont(fonts[i], size);
		if (f) return f;
	}
	return NULL;
}

static void blit_text (SDL_Renderer *r, TTF_Font *f, const char *text, int x, int y, SDL_Color col)
{
	if (!text || !*text) return;
	SDL_Surface *s = TTF_RenderText_Blended(f, text, col);
	if (!s) return;
	SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);

	if (t) {
		SDL_Rect dst = { x, y, s->w, s->h };
		SDL_RenderCopy(r, t, NULL, &dst);
		SDL_DestroyTexture(t);
	}

	SDL_FreeSurface(s);
}

static int init_window (SdlFrontend *sdl)
{
	sdl->win = SDL_CreateWindow("R2-Boy - Remap",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		REMAP_WINDOW_W, REMAP_WINDOW_H, SDL_WINDOW_RESIZABLE);

	if (!sdl->win) {
		fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
		TTF_Quit();
		SDL_Quit();
		return 0;
	}

	sdl->ren = SDL_CreateRenderer(sdl->win, -1,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if (!sdl->ren) {
		fprintf(stderr, "SDL_CreateRenderer error: %s, software fallback\n", SDL_GetError());
		sdl->ren = SDL_CreateRenderer(sdl->win, -1, SDL_RENDERER_SOFTWARE);
		if (!sdl->ren) {
			fprintf(stderr, "Software renderer also failed: %s\n", SDL_GetError());
			SDL_DestroyWindow(sdl->win);
			TTF_Quit();
			SDL_Quit();
			return 0;
		}
	}
	return 1;
}

static int init_font (SdlFrontend *sdl)
{
	TTF_Font *font = open_font(18);
	if (!font) {
		fprintf(stderr, "No usable TTF font found under /usr/share/fonts\n");
		fprintf(stderr, "Install fonts-dejavu-core or similar.\n");
		SDL_DestroyRenderer(sdl->ren);
		SDL_DestroyWindow(sdl->win);
		TTF_Quit();
		SDL_Quit();
		return 0;
	}
	sdl->font = font;
	return 1;
}

static int init_sdl (SdlFrontend *sdl)
{
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	if (TTF_Init() < 0) {
		fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
		SDL_Quit();
		return 0;
	}

	if (!init_window(sdl)) return 0;
	if (!init_font(sdl)) return 0;

	sdl->pad = NULL;
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			sdl->pad = SDL_GameControllerOpen(i);
			break;
		}
	}
	return 1;
}

static void init_consts (SdlFrontend *sdl)
{
	sdl->white = (SDL_Color){0xE8, 0xE8, 0xE8, 0xFF};
	sdl->dim = (SDL_Color){0x90, 0x90, 0x90, 0xFF};
	sdl->hl = (SDL_Color){0xFF, 0xC6, 0x3E, 0xFF};
	sdl->accent = (SDL_Color){0x6F, 0xC9, 0xF6, 0xFF};
	sdl->bad = (SDL_Color){0xE0, 0x5A, 0x5A, 0xFF};

	SDL_LogSetOutputFunction(NULL, NULL);
	SDL_GameControllerEventState(SDL_ENABLE);

	sdl->selected = 0;
	sdl->capturing = CAP_NONE;

	sdl->running = 1;
	sdl->dirty = 0;

	sdl->row_h = 30;
	sdl->list_x = 24;
	sdl->list_y = 72;
	sdl->col_label = sdl->list_x;
	sdl->col_kb = sdl->list_x + 180;
	sdl->col_pad = sdl->list_x + 380;
}

static void handle_key (SdlFrontend *sdl, Config *cfg, SDL_Event *e)
{
	if (e->key.repeat) return;
	SDL_Scancode sc = e->key.keysym.scancode;
	uint16_t mods = e->key.keysym.mod & KBMOD_ANY;

	if (sdl->capturing == CAP_KEYBOARD) {
		if (sc == SDL_SCANCODE_ESCAPE) {
			sdl->capturing = CAP_NONE;
			return;
		}
		if (is_modifier_scancode(sc)) return;
		Keybind kb = {
			.scancode = sc,
			.mods = mods,
		};
		set_kb_binding(&cfg->keymap, (Action)sdl->selected, kb);
		sdl->capturing = CAP_NONE;
		sdl->dirty = 1;
		return;
	}

	if (sc == SDL_SCANCODE_UP) sdl->selected = (sdl->selected + ACT_COUNT - 1) % ACT_COUNT;
	else if (sc == SDL_SCANCODE_DOWN) sdl->selected = (sdl->selected + 1) % ACT_COUNT;
	else if (sc == SDL_SCANCODE_RETURN) sdl->capturing = CAP_KEYBOARD;
	else if (sc == SDL_SCANCODE_TAB) {
		sdl->capturing = CAP_GAMEPAD;
		if (!sdl->pad)
			fprintf(stderr, "No gamepad connected; press ESC to cancel and connect one.\n");
	}
	else if (sc == SDL_SCANCODE_D) {
		Keymap defk;
		default_keymap(&defk);
		Padmap defp;
		default_padmap(&defp);
		set_kb_binding(&cfg->keymap, (Action)sdl->selected,
				kb_binding(&defk, (Action)sdl->selected));
		set_pad_binding(&cfg->padmap, (Action)sdl->selected,
				pad_binding(&defp, (Action)sdl->selected));
		sdl->dirty = 1;
	}
	else if (sc == SDL_SCANCODE_S) {
		if (sdl->dirty) save_config(cfg);
		sdl->running = 0;
	}
	else if (sc == SDL_SCANCODE_ESCAPE) {
		sdl->running = 0;
	}
}

static void handle_event (SdlFrontend *sdl, Config *cfg, SDL_Event *e)
{
	switch (e->type)
	{
	case SDL_QUIT:
		sdl->running = 0; return;
	
	case SDL_WINDOWEVENT:
		if (e->window.event == SDL_WINDOWEVENT_CLOSE)
			sdl->running = 0;
		return;

	case SDL_CONTROLLERDEVICEADDED:
		if (!sdl->pad) sdl->pad = SDL_GameControllerOpen(e->cdevice.which);
		return;

	case SDL_CONTROLLERDEVICEREMOVED:
		if (sdl->pad && e->cdevice.which ==
				SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl->pad))) {
			SDL_GameControllerClose(sdl->pad);
			sdl->pad = NULL;
		}
		return;

	case SDL_CONTROLLERBUTTONDOWN:
		if (sdl->capturing == CAP_GAMEPAD) {
			set_pad_binding(&cfg->padmap, (Action)sdl->selected, e->cbutton.button);
			sdl->capturing = CAP_NONE;
			sdl->dirty = 1;
		}
		return;

	case SDL_KEYDOWN: {
		handle_key(sdl, cfg, e);
		return;
	}
	}
}

static void draw (SdlFrontend *sdl, Config *cfg)
{
	SDL_SetRenderDrawColor(sdl->ren, 0x18, 0x18, 0x1E, 0xFF);
	SDL_RenderClear(sdl->ren);

	blit_text(sdl->ren, sdl->font, "R2-Boy - Key Remap", sdl->list_x, 24, sdl->white);

	blit_text(sdl->ren, sdl->font, "ACTION", sdl->col_label, sdl->list_y - 28, sdl->dim);
	blit_text(sdl->ren, sdl->font, "KEYBOARD", sdl->col_kb, sdl->list_y - 28, sdl->dim);
	blit_text(sdl->ren, sdl->font, "GAMEPAD",  sdl->col_pad, sdl->list_y - 28, sdl->dim);

	for (int i = 0; i < ACT_COUNT; i++) {
		int y = sdl->list_y + i * sdl->row_h;
		if (i == sdl->selected) {
			SDL_Rect hlrow = { sdl->list_x - 6, y - 2, REMAP_WINDOW_W - 36, sdl->row_h };
			SDL_SetRenderDrawColor(sdl->ren, 0x2A, 0x2A, 0x34, 0xFF);
			SDL_RenderFillRect(sdl->ren, &hlrow);
		}

		SDL_Color lc = (i == sdl->selected) ? sdl->hl : sdl->white;
		blit_text(sdl->ren, sdl->font, ACTIONS[i].label, sdl->col_label, y, lc);

		char tok[64];
		if (sdl->capturing == CAP_KEYBOARD && i == sdl->selected) {
			snprintf(tok, sizeof(tok), "[ press key - ESC cancels ]");
			blit_text(sdl->ren, sdl->font, tok, sdl->col_kb, y, sdl->accent);
		} else {
			write_keybind_token(kb_binding(&cfg->keymap, (Action)i), tok, sizeof(tok));
			SDL_Color c = (kb_binding(&cfg->keymap, (Action)i)).scancode
					== SDL_SCANCODE_UNKNOWN ? sdl->bad : sdl->white;
			blit_text(sdl->ren, sdl->font, tok, sdl->col_kb, y, c);
		}

		if (sdl->capturing == CAP_GAMEPAD && i == sdl->selected) {
			snprintf(tok, sizeof(tok), "[ press pad btn - ESC cancels ]");
			blit_text(sdl->ren, sdl->font, tok, sdl->col_pad, y, sdl->accent);
		} else {
			SDL_GameControllerButton b = pad_binding(&cfg->padmap, (Action)i);
			const char *bn = pad_button_name(b);
			SDL_Color c = (b == SDL_CONTROLLER_BUTTON_INVALID && i < ACT_TURBO)
					? sdl->bad : sdl->white;
			blit_text(sdl->ren, sdl->font, bn, sdl->col_pad, y, c);
		}
	}

	int fy = sdl->list_y + ACT_COUNT * sdl->row_h + 12;
	blit_text(sdl->ren, sdl->font,
		"Up/Down: select  -  Enter: capture keyboard  -  Tab: capture gamepad", sdl->list_x, fy, sdl->dim);
	blit_text(sdl->ren, sdl->font,
		"D: reset row  -  S: save & exit  -  ESC: exit without saving", sdl->list_x, fy + 22, sdl->dim);

	SDL_RenderPresent(sdl->ren);
}

void close_remap (SdlFrontend *sdl, Config *cfg)
{
	if (sdl->dirty) save_config(cfg);

	TTF_CloseFont(sdl->font);
	SDL_DestroyRenderer(sdl->ren);
	SDL_DestroyWindow(sdl->win);
	if (sdl->pad) SDL_GameControllerClose(sdl->pad);
	TTF_Quit();
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
	SDL_Quit();
	free(cfg);
	free(sdl);
}

void run_remap (void)
{
	Config *cfg = malloc(sizeof(Config));
	if (!cfg) {
		fprintf(stderr, "Not enough memory. Buy more RAM.\n");
		return;
	}
	SdlFrontend *sdl = malloc(sizeof(SdlFrontend));
	if (!sdl) {
		fprintf(stderr, "Not enough memory. Buy more RAM.\n");
		free(cfg);
		return;
	}

	init_config_defaults(cfg);
	load_config(cfg);

	if (!init_sdl(sdl)) {
		free(cfg);
		free(sdl);
		return;
	}
	init_consts(sdl);

	while (sdl->running) {
		SDL_Event e;
		while (SDL_PollEvent(&e))
			handle_event(sdl, cfg, &e);
		draw(sdl, cfg);
	}
	
	close_remap(sdl, cfg);
}
