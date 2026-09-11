/* of_sdl_extra.h -- declarations/macros for the SDL2 surface OpenJazz uses
 * that the openfpgaOS SDL2 shim (src/sdk/of_sdl2.c, src/sdk/include/SDL*.h)
 * does not provide. Never edit src/sdk/; extend it here instead.
 *
 * Force-included for every C++ TU of the cross build (src/openjazz/Makefile:
 * CXXFLAGS += -include $(PLAT)/of_sdl_extra.h, set after cxx.mk is included)
 * so upstream OpenJazz files never need to reference this header directly.
 * Because a -include'd header is processed before anything else in the TU,
 * this file must not assume <SDL.h> is already visible -- it pulls it in
 * itself. The #ifndef guards keep every definition here a no-op wherever
 * the shim (or a future, more complete one) already defines the name.
 * Force-including this into every TU (rather than the more surgical
 * per-file #include we used before) is a deliberate trade-off: it costs a
 * bit of extra preprocessing on files that don't need any of this, in
 * exchange for zero platform-specific includes inside upstream OpenJazz.
 */
#ifndef OF_SDL_EXTRA_H
#define OF_SDL_EXTRA_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- SDL_audio.h: format bitmask decode macros (io/sound.cpp) --------
 * Bit layout matches real SDL2's AUDIO_* constants (e.g. AUDIO_S16SYS =
 * 0x8010: bit 15 = signed, bit 8 = float, low byte = bit size), which the
 * shim's src/sdk/include/SDL2/SDL.h already defines with the same values.
 */
#ifndef SDL_AUDIO_MASK_BITSIZE
#define SDL_AUDIO_MASK_BITSIZE   (0xFF)
#define SDL_AUDIO_MASK_DATATYPE  (1u << 8)
#define SDL_AUDIO_MASK_SIGNED    (1u << 15)
#define SDL_AUDIO_BITSIZE(x)     ((x) & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)     ((x) & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISSIGNED(x)    ((x) & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISUNSIGNED(x)  (!SDL_AUDIO_ISSIGNED(x))
#endif

/* SDL_OpenAudioDevice's allowed_changes: the shim ignores this parameter
 * (src/sdk/of_sdl2.c, SDL_OpenAudioDevice casts it to void), so the exact
 * value doesn't affect behaviour; kept at the real SDL2 encoding. */
#ifndef SDL_AUDIO_ALLOW_ANY_CHANGE
#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x01
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE    0x02
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE  0x04
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x08
#define SDL_AUDIO_ALLOW_ANY_CHANGE \
	(SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE | \
	 SDL_AUDIO_ALLOW_CHANNELS_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE)
#endif

/* ---- SDL_pixels.h: packed-format test (io/gfx/video.cpp) --------------
 * The shim's SDL_PIXELFORMAT_* constants already use real SDL2's encoding
 * (leading nibble 0x1 = not-FourCC, next nibble = SDL_PixelType), so this
 * macro is correct against the shim's actual values, not just a stub.
 */
#ifndef SDL_ISPIXELFORMAT_PACKED
#define SDL_PIXELTYPE_PACKED8  4
#define SDL_PIXELTYPE_PACKED16 5
#define SDL_PIXELTYPE_PACKED32 6
#define SDL_PIXELFLAG(x) (((x) >> 28) & 0x0F)
#define SDL_PIXELTYPE(x) (((x) >> 24) & 0x0F)
#define SDL_ISPIXELFORMAT_FOURCC(format) ((format) && (SDL_PIXELFLAG(format) != 1))
/* Also accept SDL_PIXELFORMAT_INDEX8: it's the shim's native window/texture
 * format, but OpenJazz's texture-format loop (io/gfx/video.cpp) only
 * accepts formats this macro calls "packed", so without this it would
 * never pick INDEX8 even when SDL_GetRendererInfo (below) advertises it. */
#define SDL_ISPIXELFORMAT_PACKED(format) \
	((format) == SDL_PIXELFORMAT_INDEX8 || \
	 (!SDL_ISPIXELFORMAT_FOURCC(format) && \
	  (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED8 || \
	   SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED16 || \
	   SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32)))
#endif

/* ---- Functions missing from the shim, defined in of_sdl_extra.c ------ */
const char *SDL_GetPixelFormatName(Uint32 format);
Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

/* ---- Indexed render path (io/gfx/video.cpp) --------------------------
 * The shim's window surface is 8-bit indexed and only the palette of THAT
 * surface reaches the hardware. OpenJazz's SDL2 path picks a "packed"
 * texture format from SDL_GetRendererInfo (the shim reports none, so
 * OpenJazz falls back to RGB888) and sets its palette on its own `screen`
 * surface. Three redirects keep the whole path 8-bit and route the palette
 * to the hardware:
 *  - SDL_GetRendererInfo advertises INDEX8 as the only texture format and
 *    switches the shim to "window palette is the render palette";
 *  - SDL_ISPIXELFORMAT_PACKED (above) accepts INDEX8 so OpenJazz picks it;
 *  - SDL_SetPaletteColors mirrors OpenJazz's screen palette into the window
 *    surface palette; the screen surface is recognised by wrapping
 *    SDL_CreateRGBSurfaceWithFormatFrom, OpenJazz's only caller of it.
 * Sprite/font/menu palettes are untouched (they are not the screen's).
 */
int of_sdl_GetRendererInfo(SDL_Renderer *r, SDL_RendererInfo *info);
int of_sdl_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int first, int ncolors);
SDL_Surface *of_sdl_CreateRGBSurfaceWithFormatFrom(void *pixels, int w, int h, int depth, int pitch, Uint32 format);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_GetRendererInfo               of_sdl_GetRendererInfo
#define SDL_SetPaletteColors              of_sdl_SetPaletteColors
#define SDL_CreateRGBSurfaceWithFormatFrom of_sdl_CreateRGBSurfaceWithFormatFrom
#endif

/* ---- Window size + palette-mapped 8-bit blits (io/gfx/video.cpp etc.) --
 * SDL_SetWindowSize: the shim only records w/h; OpenJazz creates the window
 * at 320x200 and resizes to DEFAULT_SCREEN 320x288 in Video::reset(). We
 * re-create the window surface through the shim's SDL 1.2 SDL_SetVideoMode,
 * which performs the OS mode switch (320x288 = the Pocket's 10:9 panel).
 * SDL_UpperBlit (= SDL_BlitSurface): real SDL2 maps source palette indices
 * to the closest destination-palette colour when two 8-bit surfaces have
 * different palettes; OpenJazz's fonts, sprites and menus depend on that.
 * The shim copies indices verbatim, so we do the mapping here.
 */
void of_sdl_SetWindowSize(SDL_Window *win, int w, int h);
int  of_sdl_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_SetWindowSize of_sdl_SetWindowSize
#define SDL_UpperBlit     of_sdl_UpperBlit
#endif

void of_sdl_FreeSurface(SDL_Surface *s);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_FreeSurface of_sdl_FreeSurface
#endif

void of_sdl_FreePalette(SDL_Palette *palette);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_FreePalette of_sdl_FreePalette
#endif

/* ---- Unscaled RenderCopy (io/gfx/video.cpp flip()) --------------------
 * The shim's SDL_RenderCopy always goes through SDL_UpperBlitScaled, which
 * computes a 64-bit multiply and divide per pixel. OpenJazz copies the whole
 * frame through it every flip, and source and destination are the same size,
 * so the scaling is pure waste: ~92k divides per frame at 320x288. When the
 * rectangles match we take the shim's unscaled blit instead, which memcpy's
 * whole rows for same-format surfaces. The fast path only applies while no
 * render target is set (see of_sdl_SetRenderTarget below); otherwise it
 * falls back to the real SDL_RenderCopy so the active target is honoured.
 */
int of_sdl_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                      const SDL_Rect *srcrect, const SDL_Rect *dstrect);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_RenderCopy of_sdl_RenderCopy
#endif

int of_sdl_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
#ifndef OF_SDL_EXTRA_IMPL
#define SDL_SetRenderTarget of_sdl_SetRenderTarget
#endif

#ifdef __cplusplus
}
#endif

#endif /* OF_SDL_EXTRA_H */
