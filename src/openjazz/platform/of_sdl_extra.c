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

int of_sdl_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	if (!src || !dst) return -1;
	const Uint8 *map = palette_map(src, dst);
	if (!map) return SDL_UpperBlit(src, srcrect, dst, dstrect);

	/* Same clipping rules as the shim's SDL_UpperBlit. */
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
