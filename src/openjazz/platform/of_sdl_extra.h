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
#define SDL_ISPIXELFORMAT_PACKED(format) \
	(!SDL_ISPIXELFORMAT_FOURCC(format) && \
	 (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED8 || \
	  SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED16 || \
	  SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32))
#endif

/* ---- Functions missing from the shim, defined in of_sdl_extra.c ------ */
const char *SDL_GetPixelFormatName(Uint32 format);
Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

#ifdef __cplusplus
}
#endif

#endif /* OF_SDL_EXTRA_H */
