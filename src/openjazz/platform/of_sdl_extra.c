/* of_sdl_extra.c -- stubs for SDL2 calls OpenJazz makes that the openfpgaOS
 * SDL2 shim (src/sdk/of_sdl2.c) does not implement. Never edit src/sdk/.
 * Each stub returns the "nothing happened" value (0 / NULL / SDL_FALSE)
 * unless noted otherwise. See of_sdl_extra.h for the macros this shim also
 * needs to fill in (SDL_AUDIO_*, SDL_ISPIXELFORMAT_PACKED).
 */
#include <SDL.h>
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
 * BitsPerPixel/Rmask/Gmask/Bmask/Amask. Not implemented: returns UNKNOWN,
 * which src/sdk/of_sdl2.c's SDL_PixelFormatEnumToMasks (see its `default`
 * case) already treats as a valid 32bpp ARGB fallback. */
Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	(void)bpp; (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;
	return SDL_PIXELFORMAT_UNKNOWN;
}
