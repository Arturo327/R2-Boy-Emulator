#ifndef REMAP_UI_H
#define REMAP_UI_H

#include <SDL2/SDL_ttf.h>

#define REMAP_WINDOW_W 720
#define REMAP_WINDOW_H 580
#define REMAP_FOOTER_H 106

typedef struct SdlFrontend {
	SDL_Window *win;
	SDL_Renderer *ren;
	TTF_Font *font;
	SDL_GameController *pad;

	SDL_Color white;
	SDL_Color dim;
	SDL_Color hl;
	SDL_Color accent;
	SDL_Color bad;

	int selected;
	enum {
		CAP_NONE,
		CAP_KEYBOARD,
		CAP_GAMEPAD
	} capturing;

	int running;
	int dirty;

	int row_h;
	int list_x;
	int list_y;
	int col_label;
	int col_kb;
	int col_pad;

	int scroll_offset;
	int visible_rows;
	int footer_y;
} SdlFrontend;

void run_remap (void);

#endif
