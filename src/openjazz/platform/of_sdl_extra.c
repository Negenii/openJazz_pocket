/* of_sdl_extra.c -- stubs for SDL2 calls OpenJazz makes that the openfpgaOS
 * SDL2 shim (src/sdk/of_sdl2.c) does not implement. Never edit src/sdk/.
 * Each stub returns the "nothing happened" value (0 / NULL / SDL_FALSE)
 * unless noted otherwise. See of_sdl_extra.h for the macros this shim also
 * needs to fill in (SDL_AUDIO_*, SDL_ISPIXELFORMAT_PACKED).
 */
#define OF_SDL_EXTRA_IMPL
#include "of_sdl_extra.h"
#include <string.h>

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

/* OpenJazz blits `screen` into an intermediate surface and hands that to
 * SDL_UpdateTexture on the very next line (io/gfx/video.cpp flip). Both are
 * 8-bit and the same size, so we defer that blit and let the texture update
 * read `screen` instead. If the expected update never arrives, the deferred
 * copy is honoured before anything else happens and the shortcut is
 * switched off for the rest of the run. */
static SDL_Surface *g_deferred_blit_dst;
static int          g_defer_blit_ok = 1;

/* Perform a deferred blit that was not consumed, and stop deferring. */
static void of_sdl_flush_deferred_blit(void) {
	SDL_Surface *pending = g_deferred_blit_dst;
	g_deferred_blit_dst = NULL;
	g_defer_blit_ok = 0;
	if (pending && g_oj_screen) SDL_UpperBlit(g_oj_screen, NULL, pending, NULL);
}

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
	if (s && (s == g_deferred_blit_dst || s == g_oj_screen))
		of_sdl_flush_deferred_blit();
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

/* ---- palette map cache ------------------------------------------------ */
#define OF_PALMAP_SLOTS 8
static struct {
	const SDL_Palette *src, *dst;
	Uint32 src_ver, dst_ver;
	int identity;          /* 1 = palettes match, blit verbatim (no map) */
	Uint8 map[256];
} g_palmap[OF_PALMAP_SLOTS];
static int g_palmap_next;

/* Exact-colour index of the destination palette, keyed by RGB565; a hit is
 * verified against the full 8-bit colour, so 565 collisions cannot mis-map. */
static Uint8  g_dst_lut[65536];
static Uint8  g_dst_lut_set[65536 / 8];
static const SDL_Palette *g_dst_lut_pal; static Uint32 g_dst_lut_ver;

/* Flushes the map cache when OpenJazz frees a palette directly. It has no
 * such call today, and the shim's internal SDL_FreeSurface -> SDL_FreePalette
 * path is compiled separately and not redirected, so this wrapper is
 * currently unreachable; the cache's real safety net is the version check in
 * palette_map(). Kept so a future direct free cannot resurrect a stale entry. */
void of_sdl_FreePalette(SDL_Palette *palette) {
	memset(g_palmap, 0, sizeof g_palmap);
	g_dst_lut_pal = NULL;
	SDL_FreePalette(palette);
}

static inline Uint16 rgb565(SDL_Color c) { return (Uint16)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3)); }

static void rebuild_dst_lut(const SDL_Palette *p) {
	memset(g_dst_lut_set, 0, sizeof g_dst_lut_set);
	int n = p->ncolors > 256 ? 256 : p->ncolors;
	for (int i = 0; i < n; i++) {
		Uint16 k = rgb565(p->colors[i]);
		if (!(g_dst_lut_set[k >> 3] & (1 << (k & 7)))) { g_dst_lut[k] = (Uint8)i; g_dst_lut_set[k >> 3] |= (Uint8)(1 << (k & 7)); }
	}
	g_dst_lut_pal = p; g_dst_lut_ver = p->version;
}

static Uint8 find_color(const SDL_Palette *p, SDL_Color c) {
	int n = p->ncolors > 256 ? 256 : p->ncolors;
	Uint16 k = rgb565(c);
	if (g_dst_lut_set[k >> 3] & (1 << (k & 7))) {
		SDL_Color d = p->colors[g_dst_lut[k]];
		if (d.r == c.r && d.g == c.g && d.b == c.b) return g_dst_lut[k];
	}
	/* Exact scan (handles 565 collisions), then nearest by squared distance. */
	for (int i = 0; i < n; i++) { SDL_Color d = p->colors[i]; if (d.r == c.r && d.g == c.g && d.b == c.b) return (Uint8)i; }
	int best = 0; unsigned bestd = ~0u;
	for (int i = 0; i < n; i++) {
		int dr = (int)p->colors[i].r - c.r, dg = (int)p->colors[i].g - c.g, db = (int)p->colors[i].b - c.b;
		unsigned d = (unsigned)(dr*dr + dg*dg + db*db);
		if (d < bestd) { bestd = d; best = i; if (!d) break; }
	}
	return (Uint8)best;
}

/* NULL when no remap is needed (same palette object or identical colours). */
static const Uint8 *palette_map(const SDL_Surface *src, const SDL_Surface *dst) {
	if (!src->format || !dst->format) return NULL;
	if (src->format->BytesPerPixel != 1 || dst->format->BytesPerPixel != 1) return NULL;
	const SDL_Palette *sp = src->format->palette, *dp = dst->format->palette;
	if (!sp || !dp || sp == dp) return NULL;
	/* A palette nobody ever populated (version 0 = calloc'd by
	 * SDL_AllocPalette) belongs to the render chain: textureSurface,
	 * texture and the window surface carry indices, not colours -- the
	 * palette reaches the hardware separately at present time. Mapping
	 * into those all-black tables would collapse the frame to one index. */
	if (sp->version == 0 || dp->version == 0) return NULL;

	for (int i = 0; i < OF_PALMAP_SLOTS; i++) {
		if (g_palmap[i].src == sp && g_palmap[i].dst == dp &&
		    g_palmap[i].src_ver == sp->version && g_palmap[i].dst_ver == dp->version)
			return g_palmap[i].identity ? NULL : g_palmap[i].map;
	}

	int n = sp->ncolors < dp->ncolors ? sp->ncolors : dp->ncolors;
	if (n > 256) n = 256;
	int identical = memcmp(sp->colors, dp->colors, (size_t)n * sizeof(SDL_Color)) == 0;

	int slot = g_palmap_next; g_palmap_next = (g_palmap_next + 1) % OF_PALMAP_SLOTS;
	g_palmap[slot].src = sp; g_palmap[slot].dst = dp; g_palmap[slot].src_ver = sp->version; g_palmap[slot].dst_ver = dp->version;
	g_palmap[slot].identity = identical;
	if (identical) return NULL;

	if (g_dst_lut_pal != dp || g_dst_lut_ver != dp->version) rebuild_dst_lut(dp);
	int sn = sp->ncolors > 256 ? 256 : sp->ncolors;
	for (int i = 0; i < 256; i++) g_palmap[slot].map[i] = i < sn ? find_color(dp, sp->colors[i]) : (Uint8)i;
	return g_palmap[slot].map;
}

/* ---- Opt-in frame-time breakdown (`make PERF=1`), task-10b ------------
 * Counts what happens thousands of times a frame (blitted pixels, split
 * by path; bytes moved by the texture update and the window copy)
 * instead of timing it -- of_time_us() is an ecall -- and times directly
 * only what happens tens of times a second: the whole frame, and the
 * audio pump (where OpenJazz's xmp mixing runs, in the callback we own).
 * A one-time calibration converts the per-pixel counts to milliseconds.
 * Every call site below collapses to nothing when OF_PERF is off, so the
 * release build pays for none of this: no clock read, no counter update,
 * no extra branch survives the preprocessor.
 */
#if defined(OF_PERF) && !defined(OF_PC)
#include "of_timer.h"     /* of_time_us(): an ecall, so call it rarely */
#include "of_video.h"     /* of_video_set_display_mode() */
#include <stdio.h>
#include <unistd.h>

static unsigned long long g_perf_ck_px, g_perf_op_px;  /* pixels, by blit path */
static unsigned long long g_perf_tex_b, g_perf_cpy_b;  /* bytes moved */
static unsigned g_perf_blits;                          /* blit calls this window */
static unsigned g_perf_frames;                         /* frames this window */
static unsigned g_perf_frame_us, g_perf_aud_us;        /* accumulated this window */
static unsigned g_perf_prev_us;
static int      g_perf_have_prev;
static unsigned g_perf_ck_ns_px, g_perf_op_ns_px;      /* calibrated once, ns/pixel */
static int      g_perf_calibrated;
static int      g_perf_overlay_on;

/* Both paths cost a fixed amount per pixel; measure it once so the cheap
 * per-frame pixel counts can be converted to milliseconds. Called lazily
 * on the first frame (not at load time), so the video mode is already
 * up. The SDL_* names below are the real shim functions: the redirect
 * macros in of_sdl_extra.h are inactive in this file (OF_SDL_EXTRA_IMPL
 * is defined above), which is what we want -- we are measuring the
 * shim's own blit loops, the same ones the game's blits fall through to
 * whenever our layer doesn't have a faster path of its own. */
static void of_perf_calibrate(void) {
	SDL_Surface *src = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 8, SDL_PIXELFORMAT_INDEX8);
	SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 8, SDL_PIXELFORMAT_INDEX8);
	if (!src || !dst) {
		if (src) SDL_FreeSurface(src);
		if (dst) SDL_FreeSurface(dst);
		return; /* rates stay 0: the overlay prints nothing */
	}
	/* A realistic tile: mostly opaque with some key pixels. */
	memset(src->pixels, 7, (size_t)src->pitch * src->h);
	for (int y = 0; y < src->h; y += 4)
		memset((Uint8 *)src->pixels + (size_t)y * src->pitch, 0, 8);

	const int reps = 400;
	SDL_Rect sr = {0, 0, 32, 32}, dr = {0, 0, 32, 32};

	SDL_SetColorKey(src, SDL_TRUE, 0);
	unsigned t0 = of_time_us();
	for (int i = 0; i < reps; i++) { SDL_Rect d = dr; SDL_UpperBlit(src, &sr, dst, &d); }
	unsigned t1 = of_time_us();

	SDL_SetColorKey(src, SDL_FALSE, 0);
	for (int i = 0; i < reps; i++) { SDL_Rect d = dr; SDL_UpperBlit(src, &sr, dst, &d); }
	unsigned t2 = of_time_us();

	/* nanoseconds per pixel, kept as integers */
	unsigned px = (unsigned)reps * 32u * 32u;
	g_perf_ck_ns_px = px ? ((t1 - t0) * 1000u) / px : 0;
	g_perf_op_ns_px = px ? ((t2 - t1) * 1000u) / px : 0;
	SDL_FreeSurface(src); SDL_FreeSurface(dst);
}

/* Pixel counters: incremented after clipping is known, using the same
 * clipped w*h the blit will actually touch, and the same colorkey/bpp
 * test of_sdl2.c's SDL_UpperBlit uses to choose its row-memcpy versus
 * per-pixel path (src/dst/of_sdl2.c ~lines 390/396) -- computed here
 * independently of which branch of of_sdl_UpperBlit below actually does
 * the copy (the palette-remap loop or a fall-through to the shim), so
 * the count never depends on that choice. A deferred (skipped) blit
 * never reaches this call: its pixels are accounted for once, later, as
 * the bytes SDL_UpdateTexture reads straight out of `screen`. */
static void of_perf_count_blit(SDL_Surface *src, const SDL_Rect *srcrect,
                                SDL_Surface *dst, const SDL_Rect *dstrect) {
	g_perf_blits++;
	if (!src->format || !dst->format) return;
	if (src->format->BytesPerPixel != 1 || dst->format->BytesPerPixel != 1) return;

	SDL_Rect sr;
	if (srcrect) sr = *srcrect; else { sr.x = 0; sr.y = 0; sr.w = src->w; sr.h = src->h; }
	int dx = dstrect ? dstrect->x : 0, dy = dstrect ? dstrect->y : 0;
	if (sr.x < 0) { dx -= sr.x; sr.w += sr.x; sr.x = 0; }
	if (sr.y < 0) { dy -= sr.y; sr.h += sr.y; sr.y = 0; }
	if (sr.x + sr.w > src->w) sr.w = src->w - sr.x;
	if (sr.y + sr.h > src->h) sr.h = src->h - sr.y;
	SDL_Rect cl = dst->clip_rect;
	if (dx < cl.x) { int d = cl.x - dx; sr.w -= d; dx = cl.x; }
	if (dy < cl.y) { int d = cl.y - dy; sr.h -= d; dy = cl.y; }
	if (dx + sr.w > cl.x + cl.w) sr.w = cl.x + cl.w - dx;
	if (dy + sr.h > cl.y + cl.h) sr.h = cl.y + cl.h - dy;
	if (sr.w <= 0 || sr.h <= 0) return; /* nothing will actually be touched */

	Uint32 key = 0;
	int ck = (SDL_GetColorKey(src, &key) == 0);
	unsigned long long px = (unsigned long long)sr.w * (unsigned long long)sr.h;
	if (ck) g_perf_ck_px += px; else g_perf_op_px += px;
}

/* Bytes handed to SDL_UpdateTexture: the rect (or the whole texture)
 * times the texture's own bytes/pixel, matching what of_sdl2.c's
 * SDL_UpdateTexture actually copies. */
static void of_perf_count_tex(SDL_Texture *texture, const SDL_Rect *rect) {
	Uint32 format; int access, tw, th;
	if (!texture || SDL_QueryTexture(texture, &format, &access, &tw, &th) != 0) return;
	int w = rect ? rect->w : tw, h = rect ? rect->h : th;
	if (w <= 0 || h <= 0) return;
	int bpp = SDL_BITSPERPIXEL(format) / 8;
	if (bpp <= 0) bpp = 1;
	g_perf_tex_b += (unsigned long long)w * (unsigned long long)h * (unsigned long long)bpp;
}

#define OF_PERF_COUNT_BLIT(s, sr, d, dr) of_perf_count_blit((s), (sr), (d), (dr))
#define OF_PERF_TEX_BYTES(t, r)          of_perf_count_tex((t), (r))
#define OF_PERF_CPY_BYTES(w, h, bpp) \
	(g_perf_cpy_b += (unsigned long long)(w) * (unsigned long long)(h) * (unsigned long long)(bpp))
#define OF_PERF_AUDIO_BEGIN() unsigned __of_perf_t0 = of_time_us()
#define OF_PERF_AUDIO_END()   (g_perf_aud_us += (unsigned)(of_time_us() - __of_perf_t0))

/* Once the accumulated frame time passes 1s: convert the counts to
 * milliseconds, print one overlay line, then reset every counter.
 * Nothing is printed while the calibration rates are still zero (the
 * calibration surfaces failed to allocate). */
static void of_perf_present(void) {
	if (!g_perf_calibrated) {
		/* Terminal text over the framebuffer, so the numbers are readable
		 * without a debug cable. Switched on before anything can fail. */
		of_video_set_display_mode(2);
		of_perf_calibrate();
		g_perf_calibrated = 1;
	}

	unsigned now = of_time_us();
	if (g_perf_have_prev) g_perf_frame_us += (unsigned)(now - g_perf_prev_us);
	g_perf_prev_us = now;
	g_perf_have_prev = 1;
	g_perf_frames++;

	/* Trigger on a frame count, not on elapsed time: if the microsecond
	 * clock were unavailable the time-based trigger would never fire and we
	 * would learn nothing at all. Print the raw numbers unconditionally and
	 * let the reader judge them. The terminal is 40 columns wide at this
	 * resolution, so keep each line short. */
	if (g_perf_frames < 60) return;

	char line[96];
	int n = snprintf(line, sizeof line,
		"[p] %uf dt=%ums aud=%ums\n",
		g_perf_frames, g_perf_frame_us / 1000u, g_perf_aud_us / 1000u);
	if (n > 0) { ssize_t w = write(1, line, (size_t)n); (void)w; }

	n = snprintf(line, sizeof line,
		"[p] ck=%uk op=%uk cp=%uk ns=%u/%u\n",
		(unsigned)(g_perf_ck_px / 1000ull), (unsigned)(g_perf_op_px / 1000ull),
		(unsigned)((g_perf_tex_b + g_perf_cpy_b) / 1000ull),
		g_perf_ck_ns_px, g_perf_op_ns_px);
	if (n > 0) { ssize_t w = write(1, line, (size_t)n); (void)w; }

	g_perf_frame_us = 0; g_perf_aud_us = 0; g_perf_frames = 0; g_perf_blits = 0;
	g_perf_ck_px = 0; g_perf_op_px = 0; g_perf_tex_b = 0; g_perf_cpy_b = 0;
}
#define OF_PERF_PRESENT() of_perf_present()

#else /* !(OF_PERF && !OF_PC): every hook below is a no-op, nothing survives */
#define OF_PERF_COUNT_BLIT(s, sr, d, dr) ((void)0)
#define OF_PERF_TEX_BYTES(t, r)          ((void)0)
#define OF_PERF_CPY_BYTES(w, h, bpp)     ((void)0)
#define OF_PERF_AUDIO_BEGIN()            ((void)0)
#define OF_PERF_AUDIO_END()              ((void)0)
#define OF_PERF_PRESENT()                ((void)0)
#endif

/* Whether any byte of w equals the key byte replicated in kk. The classic
 * has-zero-byte test: after xoring, a zero byte marks a match. */
static inline int of_word_has_key(Uint32 w, Uint32 kk) {
	Uint32 v = w ^ kk;
	return (int)((v - 0x01010101u) & ~v & 0x80808080u);
}

int of_sdl_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	if (!src || !dst) return -1;

	if (g_defer_blit_ok && !srcrect && !dstrect && src == g_oj_screen
	    && src->format && dst->format
	    && src->format->BytesPerPixel == 1 && dst->format->BytesPerPixel == 1
	    && dst->w == src->w && dst->h == src->h) {
		if (g_deferred_blit_dst) {
			/* A previous deferral was never consumed: honour it, give up
			 * on deferring, and let this call copy for real. */
			of_sdl_flush_deferred_blit();
		} else {
			g_deferred_blit_dst = dst;
			return 0;
		}
	} else if (g_deferred_blit_dst && dst == g_deferred_blit_dst) {
		/* Someone is writing into the surface whose copy we deferred. */
		of_sdl_flush_deferred_blit();
	}

	OF_PERF_COUNT_BLIT(src, srcrect, dst, dstrect);

	const Uint8 *map = palette_map(src, dst);

	/* Same clipping rules as the shim's SDL_UpperBlit. Shared by the
	 * palette-remap loop below and the opaque-run copy in the !map case;
	 * computing it here costs nothing when neither owns the blit and we
	 * fall through to the real SDL_UpperBlit, since that call recomputes
	 * the identical clip from the untouched srcrect/dstrect anyway. */
	SDL_Rect sr;
	if (srcrect) sr = *srcrect; else { sr.x = 0; sr.y = 0; sr.w = src->w; sr.h = src->h; }
	int dx = dstrect ? dstrect->x : 0, dy = dstrect ? dstrect->y : 0;
	if (sr.x < 0) { dx -= sr.x; sr.w += sr.x; sr.x = 0; }
	if (sr.y < 0) { dy -= sr.y; sr.h += sr.y; sr.y = 0; }
	if (sr.x + sr.w > src->w) sr.w = src->w - sr.x;
	if (sr.y + sr.h > src->h) sr.h = src->h - sr.y;
	SDL_Rect cl = dst->clip_rect;
	if (dx < cl.x) { int d = cl.x - dx; sr.x += d; sr.w -= d; dx = cl.x; }
	if (dy < cl.y) { int d = cl.y - dy; sr.y += d; sr.h -= d; dy = cl.y; }
	if (dx + sr.w > cl.x + cl.w) sr.w = cl.x + cl.w - dx;
	if (dy + sr.h > cl.y + cl.h) sr.h = cl.y + cl.h - dy;

	if (!map) {
		/* No index remapping needed. If both surfaces are 1 byte/pixel and
		 * the source has a colorkey, this is exactly the shim's per-pixel
		 * colorkey path (src/sdk/of_sdl2.c) -- take over that copy here
		 * instead of falling through to it, so we can skip runs instead of
		 * testing every pixel. Anything else (different bpp, no colorkey,
		 * mixed formats) keeps falling through to the real SDL_UpperBlit
		 * exactly as before: call it with the original, untouched
		 * srcrect/dstrect so it repeats its own clipping and any side
		 * effects (e.g. g_render_palette tracking) unchanged. */
		if (src->format && dst->format
		    && src->format->BytesPerPixel == 1 && dst->format->BytesPerPixel == 1) {
			Uint32 key = 0;
			if (SDL_GetColorKey(src, &key) == 0) {
				if (sr.w <= 0 || sr.h <= 0) { if (dstrect) { dstrect->w = 0; dstrect->h = 0; } return 0; }

				/* Art in this game is mostly opaque with a colorkey around it, and the
				 * shim's colorkey path tests and stores one byte at a time. Copy the
				 * runs between key pixels instead: a fully opaque row becomes a single
				 * memcpy, which moves words rather than bytes. */
				Uint8 k = (Uint8)key;
				Uint32 kk = (Uint32)k * 0x01010101u;
				for (int y = 0; y < sr.h; y++) {
					const Uint8 *sp = (const Uint8 *)src->pixels + (size_t)(sr.y + y) * src->pitch + sr.x;
					Uint8 *dp = (Uint8 *)dst->pixels + (size_t)(dy + y) * dst->pitch + dx;
					int x = 0;
					while (x < sr.w) {
						while (x < sr.w && sp[x] == k) x++;          /* skip transparent */
						if (x >= sr.w) break;
						int start = x;
						/* Byte-wise until the source is word-aligned, then test four
						 * pixels per iteration; art is mostly opaque, so most words
						 * contain no key byte and the scan advances four at a time. */
						while (x < sr.w && ((uintptr_t)(sp + x) & 3u) && sp[x] != k) x++;
						while (x + 4 <= sr.w && !((uintptr_t)(sp + x) & 3u)) {
							Uint32 w;
							memcpy(&w, sp + x, sizeof w);
							if (of_word_has_key(w, kk)) break;
							x += 4;
						}
						while (x < sr.w && sp[x] != k) x++;           /* one opaque run */
						memcpy(dp + start, sp + start, (size_t)(x - start));
					}
				}
				if (dstrect) { dstrect->w = sr.w; dstrect->h = sr.h; }
				return 0;
			}
		}
		return SDL_UpperBlit(src, srcrect, dst, dstrect);
	}

	if (sr.w <= 0 || sr.h <= 0) { if (dstrect) { dstrect->w = 0; dstrect->h = 0; } return 0; }

	Uint32 key = 0; int ck = (SDL_GetColorKey(src, &key) == 0);
	for (int y = 0; y < sr.h; y++) {
		const Uint8 *sp = (const Uint8 *)src->pixels + (size_t)(sr.y + y) * src->pitch + sr.x;
		Uint8 *dp = (Uint8 *)dst->pixels + (size_t)(dy + y) * dst->pitch + dx;
		if (ck) { for (int x = 0; x < sr.w; x++) { Uint8 v = sp[x]; if (v == (Uint8)key) continue; dp[x] = map[v]; } }
		else    { for (int x = 0; x < sr.w; x++) dp[x] = map[sp[x]]; }
	}
	if (dstrect) { dstrect->w = sr.w; dstrect->h = sr.h; }
	return 0;
}

/* The shim has no SDL_GetRenderTarget and SDL_Renderer is opaque, so we
 * remember the target ourselves: of_sdl_RenderCopy's fast path writes to the
 * window surface, which is only the right destination while no render target
 * is set. Nothing in OpenJazz sets one today; this keeps the wrapper honest
 * if that ever changes. */
static SDL_Texture *g_render_target;

int of_sdl_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
	g_render_target = texture;
	return SDL_SetRenderTarget(renderer, texture);
}

int of_sdl_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                      const SDL_Rect *srcrect, const SDL_Rect *dstrect) {
	SDL_Surface *dst = SDL_GetWindowSurface(NULL);
	Uint32 format; int access, tw, th;
	if (!g_render_target && dst && texture && SDL_QueryTexture(texture, &format, &access, &tw, &th) == 0) {
		SDL_Rect sr, dr;
		if (srcrect) sr = *srcrect; else { sr.x = 0; sr.y = 0; sr.w = tw; sr.h = th; }
		if (dstrect) dr = *dstrect; else { dr.x = 0; dr.y = 0; dr.w = dst->w; dr.h = dst->h; }
		if (sr.w == dr.w && sr.h == dr.h && sr.w > 0 && sr.h > 0) {
			void *pixels = NULL; int pitch = 0;
			if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0 && pixels) {
				SDL_Surface *view = SDL_CreateRGBSurfaceWithFormatFrom(
					pixels, tw, th, SDL_BITSPERPIXEL(format), pitch, format);
				if (view) {
					SDL_Rect d = dr;
					OF_PERF_CPY_BYTES(sr.w, sr.h, SDL_BITSPERPIXEL(format) / 8);
					int rc = SDL_UpperBlit(view, &sr, dst, &d);
					SDL_FreeSurface(view);
					SDL_UnlockTexture(texture);
					return rc;
				}
				SDL_UnlockTexture(texture);
			}
		}
	}
	return SDL_RenderCopy(renderer, texture, srcrect, dstrect);
}

int of_sdl_RenderClear(SDL_Renderer *renderer) {
	if (!g_render_target) return 0;          /* window: the copy overwrites it */
	return SDL_RenderClear(renderer);
}

int of_sdl_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                         const void *pixels, int pitch) {
	OF_PERF_TEX_BYTES(texture, rect);
	if (g_deferred_blit_dst) {
		SDL_Surface *pending = g_deferred_blit_dst;
		if (g_oj_screen && pixels == pending->pixels) {
			/* The expected call: read the frame from `screen`, which the
			 * deferred blit would only have duplicated. */
			g_deferred_blit_dst = NULL;
			return SDL_UpdateTexture(texture, rect, g_oj_screen->pixels,
			                         g_oj_screen->pitch);
		}
		of_sdl_flush_deferred_blit();
	}
	return SDL_UpdateTexture(texture, rect, pixels, pitch);
}

/* ---- Bounded audio latency -------------------------------------------
 * OpenJazz opens audio at 24 kHz (SOUND_FREQ, src/openjazz/src/platforms/
 * openfpga.h) with samples=1024 (SOUND_SAMPLES). We hand the shim's real
 * SDL_OpenAudioDevice a callback-less spec so its own audio_pump() (which
 * bails out whenever __sdl_audio_cb is null) never runs, keep the real
 * callback here, and drive it ourselves from of_sdl_Delay/of_sdl_RenderPresent
 * -- the same two call sites the shim's pump used for its own topping-up.
 * SDL_GetQueuedAudioSize/SDL_QueueAudio (both real shim calls once this file
 * is done macro-redirecting) read and write the very same ring, so this is
 * just a smaller, self-imposed fill target instead of "always full".
 */
static SDL_AudioCallback g_audio_cb;
static void             *g_audio_userdata;
static SDL_AudioDeviceID g_audio_dev;
static int               g_audio_paused = 1;
static int               g_audio_target_pairs;   /* queue depth we maintain */

/* Top the ring up to g_audio_target_pairs stereo pairs, no further. */
static void of_sdl_audio_pump(void) {
	static int16_t buf[2048];            /* 1024 stereo pairs */
	if (!g_audio_cb || g_audio_paused || !g_audio_dev) return;
	OF_PERF_AUDIO_BEGIN();
	for (int guard = 0; guard < 8; guard++) {
		int queued = (int)(SDL_GetQueuedAudioSize(g_audio_dev) / 4);
		int want = g_audio_target_pairs - queued;
		if (want <= 0) break;
		if (want > 1024) want = 1024;
		g_audio_cb(g_audio_userdata, (Uint8 *)buf, want * 4);
		SDL_QueueAudio(g_audio_dev, buf, (Uint32)want * 4);
	}
	OF_PERF_AUDIO_END();
}

SDL_AudioDeviceID of_sdl_OpenAudioDevice(const char *device, int iscapture,
                           const SDL_AudioSpec *desired, SDL_AudioSpec *obtained,
                           int allowed_changes) {
	if (!desired) return 0;
	/* Hand the shim a callback-less spec so its own pump never runs; keep
	 * the callback here and drive it from of_sdl_audio_pump(). */
	SDL_AudioSpec quiet = *desired;
	quiet.callback = NULL;
	quiet.userdata = NULL;
	SDL_AudioSpec got;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice(device, iscapture, &quiet,
	                                            obtained ? obtained : &got,
	                                            allowed_changes);
	if (!dev) return 0;
	const SDL_AudioSpec *result = obtained ? obtained : &got;
	g_audio_cb = desired->callback;
	g_audio_userdata = desired->userdata;
	g_audio_dev = dev;
	g_audio_paused = 1;
	/* Four of the game's own buffers: 4 x 1024 pairs at 24 kHz is ~170 ms,
	 * enough to ride out a long frame, short enough to feel immediate. */
	g_audio_target_pairs = (result->samples > 0 ? result->samples : 1024) * 4;
	return dev;
}

void of_sdl_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
	g_audio_paused = pause_on ? 1 : 0;
	SDL_PauseAudioDevice(dev, pause_on);
	if (!g_audio_paused) of_sdl_audio_pump();
}

void of_sdl_CloseAudioDevice(SDL_AudioDeviceID dev) {
	g_audio_cb = NULL; g_audio_userdata = NULL;
	g_audio_dev = 0; g_audio_paused = 1;
	SDL_CloseAudioDevice(dev);
}

void of_sdl_Delay(Uint32 ms) {
	/* Sleep in slices so the queue is topped up during the wait, not just
	 * around it: with a ~171 ms target, one uninterrupted SDL_Delay longer
	 * than that would drain the ring mid-sleep. Cutscene frame delays
	 * (jj1scene.cpp) are data-driven and can exceed it. */
	of_sdl_audio_pump();
	while (ms > 0) {
		Uint32 slice = ms > 10 ? 10 : ms;
		SDL_Delay(slice);
		ms -= slice;
		of_sdl_audio_pump();
	}
}

void of_sdl_RenderPresent(SDL_Renderer *renderer) {
	of_sdl_audio_pump();
	OF_PERF_PRESENT();
	SDL_RenderPresent(renderer);
}
