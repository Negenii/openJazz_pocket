/* of_sdl_extra.c -- stubs for SDL2 calls OpenJazz makes that the openfpgaOS
 * SDL2 shim (src/sdk/of_sdl2.c) does not implement. Never edit src/sdk/.
 * Each stub returns the "nothing happened" value (0 / NULL / SDL_FALSE)
 * unless noted otherwise. See of_sdl_extra.h for the macros this shim also
 * needs to fill in (SDL_AUDIO_*, SDL_ISPIXELFORMAT_PACKED).
 */
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
