/* of_sdl_extra.c -- stubs for SDL2 calls OpenJazz makes that the openfpgaOS
 * SDL2 shim (src/sdk/of_sdl2.c) does not implement. Never edit src/sdk/.
 * Each stub returns the "nothing happened" value (0 / NULL / SDL_FALSE)
 * unless noted otherwise. See of_sdl_extra.h for the macros this shim also
 * needs to fill in (SDL_AUDIO_*, SDL_ISPIXELFORMAT_PACKED).
 */
#define OF_SDL_EXTRA_IMPL
#include "of_sdl_extra.h"

/* io/gfx/video.cpp: LOG_TRACE("... '%s' ...", SDL_GetPixelFormatName(fmt)).
 * Real SDL2 returns a name string, never NULL; this shim has no format
 * name table, so return a fixed placeholder instead of NULL to keep the
 * %s format specifier safe. */
const char *SDL_GetPixelFormatName(Uint32 format) {
	(void)format;
	return "unknown";
}

/* io/gfx/video.cpp: derives the screen surface's SDL_PixelFormat enum from
 * BitsPerPixel/Rmask/Gmask/Bmask/Amask (canvas->format->...). This is a real
 * implementation, not a stub: returning SDL_PIXELFORMAT_UNKNOWN here is
 * wrong, not just imprecise -- src/sdk/of_sdl2.c's SDL_PixelFormatEnumToMasks
 * `default` case reports UNKNOWN as 32bpp, so the SDL_CreateRGBSurfaceWithFormatFrom
 * call right after this one would think the canvas is 4 bytes/pixel while
 * the real buffer and pitch are 1 byte/pixel (INDEX8), and SDL_BlitSurface
 * in flip() would read 4x past the end of every row.
 *
 * OpenJazz only ever calls this once, for the INDEX8 canvas (bpp=8, all
 * masks 0) -- handled directly. For completeness (and because a second call
 * site could show up upstream), also search the RGB/RGBA formats the shim's
 * SDL_PixelFormatEnumToMasks recognizes and return the first exact match;
 * SDL_PIXELFORMAT_UNKNOWN only if nothing matches. */
Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	if (bpp == 8 && Rmask == 0 && Gmask == 0 && Bmask == 0 && Amask == 0)
		return SDL_PIXELFORMAT_INDEX8;

	static const Uint32 candidates[] = {
		SDL_PIXELFORMAT_RGB565,
		SDL_PIXELFORMAT_RGB888,
		SDL_PIXELFORMAT_RGBX8888,
		SDL_PIXELFORMAT_BGR888,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_PIXELFORMAT_ABGR8888,
		SDL_PIXELFORMAT_BGRA8888,
	};
	int n = (int)(sizeof(candidates) / sizeof(candidates[0]));
	for (int i = 0; i < n; i++) {
		int cbpp; Uint32 cr, cg, cb, ca;
		SDL_PixelFormatEnumToMasks(candidates[i], &cbpp, &cr, &cg, &cb, &ca);
		if (cbpp == bpp && cr == Rmask && cg == Gmask && cb == Bmask && ca == Amask)
			return candidates[i];
	}
	return SDL_PIXELFORMAT_UNKNOWN;
}

/* OpenJazz's `screen` surface (wraps canvas->pixels); see of_sdl_extra.h. */
static SDL_Surface *g_oj_screen;

int of_sdl_GetRendererInfo(SDL_Renderer *r, SDL_RendererInfo *info) {
	int rc = SDL_GetRendererInfo(r, info);
	if (rc == 0 && info) {
		info->num_texture_formats = 1;
		info->texture_formats[0] = SDL_PIXELFORMAT_INDEX8;
	}
	/* From now on present_screen() pushes the window-surface palette. */
	of_sdl_set_screen_palette_is_render(1);
	return rc;
}

SDL_Surface *of_sdl_CreateRGBSurfaceWithFormatFrom(void *pixels, int w, int h, int depth, int pitch, Uint32 format) {
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, depth, pitch, format);
	g_oj_screen = s;
	return s;
}

int of_sdl_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int first, int ncolors) {
	int rc = SDL_SetPaletteColors(palette, colors, first, ncolors);
	if (rc == 0 && g_oj_screen && g_oj_screen->format && palette == g_oj_screen->format->palette) {
		SDL_Surface *win = SDL_GetWindowSurface(NULL);
		if (win && win->format && win->format->palette && win->format->palette != palette)
			SDL_SetPaletteColors(win->format->palette, colors, first, ncolors);
	}
	return rc;
}

void of_sdl_FreeSurface(SDL_Surface *s) {
	if (s == g_oj_screen) g_oj_screen = NULL;
	SDL_FreeSurface(s);
}

void of_sdl_SetWindowSize(SDL_Window *win, int w, int h) {
	int cw = 0, ch = 0;
	SDL_GetWindowSize(win, &cw, &ch);
	if (cw == w && ch == h) return;
	/* Re-create the window surface at the new size (mode switch inside). */
	SDL_SetVideoMode(w, h, 8, 0);
}
