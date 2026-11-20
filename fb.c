/*  fb.c — framebuffer 1 bit, trames, texte rendu en 1 bit par Pango.
 *  Aucune dependance graphique hors cairo/pango : utilisable sans afficheur
 *  (c'est ce qui permet les tests et --shot).
 */
#include "oldmac.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
void fb_init(fb_t *f, int w, int h)
{
    f->w = w; f->h = h; f->stride = w;
    f->p = calloc((size_t)w * (size_t)h, 1);
}

void fb_free(fb_t *f) { free(f->p); f->p = NULL; }

void fb_clear(fb_t *f) { memset(f->p, 0, (size_t)f->w * (size_t)f->h); }
void fb_fill(fb_t *f)  { memset(f->p, 1, (size_t)f->w * (size_t)f->h); }

void fb_pset(fb_t *f, int x, int y, int v)
{
    if (x < 0 || y < 0 || x >= f->w || y >= f->h) return;
    f->p[(size_t)y * f->stride + x] = (uint8_t)(v ? 1 : 0);
}

int fb_pget(fb_t *f, int x, int y)
{
    if (x < 0 || y < 0 || x >= f->w || y >= f->h) return 0;
    return f->p[(size_t)y * f->stride + x];
}

void fb_rect(fb_t *f, int x, int y, int w, int h, int v)
{
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > f->w ? f->w : x + w;
    int y1 = y + h > f->h ? f->h : y + h;
    for (int j = y0; j < y1; j++) {
        memset(f->p + (size_t)j * f->stride + x0, v ? 1 : 0, (size_t)(x1 - x0));
    }
}

void fb_hline(fb_t *f, int x, int y, int len, int v) { fb_rect(f, x, y, len, 1, v); }
void fb_vline(fb_t *f, int x, int y, int len, int v) { fb_rect(f, x, y, 1, len, v); }

void fb_frame(fb_t *f, int x, int y, int w, int h)
{
    fb_hline(f, x, y, w, 1);
    fb_hline(f, x, y + h - 1, w, 1);
    fb_vline(f, x, y, h, 1);
    fb_vline(f, x + w - 1, y, h, 1);
}

void fb_line(fb_t *f, int x0, int y0, int x1, int y1, int v)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_pset(f, x0, y0, v);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_circle(fb_t *f, int cx, int cy, int r, int v)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) fb_pset(f, cx + x, cy + y, v);
}

void fb_rrect(fb_t *f, int x, int y, int w, int h, int r, int v)
{
    if (r < 1) { fb_rect(f, x, y, w, h, v); return; }
    fb_rect(f, x + r, y, w - 2 * r, h, v);
    fb_rect(f, x, y + r, r, h - 2 * r, v);
    fb_rect(f, x + w - r, y + r, r, h - 2 * r, v);
    fb_circle(f, x + r, y + r, r, v);
    fb_circle(f, x + w - 1 - r, y + r, r, v);
    fb_circle(f, x + r, y + h - 1 - r, r, v);
    fb_circle(f, x + w - 1 - r, y + h - 1 - r, r, v);
}

/* cadre arrondi : on dessine plein puis on evide l'interieur */
void fb_rframe(fb_t *f, int x, int y, int w, int h, int r)
{
    fb_t t; fb_init(&t, w, h);
    fb_rrect(&t, 0, 0, w, h, r, 1);
    fb_rrect(&t, 1, 1, w - 2, h - 2, r > 1 ? r - 1 : 1, 0);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (t.p[(size_t)j * t.stride + i]) fb_pset(f, x + i, y + j, 1);
    fb_free(&t);
}

static void trames(fb_t *f, int x, int y, int w, int h, int px, int py, int phase)
{
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= f->h) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= f->w) continue;
            int on = (((i + phase) % px) == 0) && (((j + phase) % py) == 0);
            if (on) f->p[(size_t)j * f->stride + i] = 1;
        }
    }
}

void fb_gray(fb_t *f, int x, int y, int w, int h)      { trames(f, x, y, w, h, 2, 2, 0); }
void fb_darkgray(fb_t *f, int x, int y, int w, int h)  { trames(f, x, y, w, h, 2, 2, 0); trames(f, x, y, w, h, 2, 2, 1); }
void fb_lightgray(fb_t *f, int x, int y, int w, int h) { trames(f, x, y, w, h, 2, 2, 1); }

void fb_invert(fb_t *f, int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            if (i < 0 || j < 0 || i >= f->w || j >= f->h) continue;
            f->p[(size_t)j * f->stride + i] ^= 1;
        }
}

void fb_copy(fb_t *dst, fb_t *src, int dx, int dy)
{
    for (int j = 0; j < src->h; j++)
        for (int i = 0; i < src->w; i++)
            fb_pset(dst, dx + i, dy + j, src->p[(size_t)j * src->stride + i]);
}

int fb_ink(fb_t *f, int x, int y, int w, int h)
{
    int n = 0;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) n += fb_pget(f, i, j);
    return n;
}

/* ------------------------------------------------------------------ */
/* texte                                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    char *txt;
    int   size, style, w, h;
    uint8_t *mask;
    unsigned long used;
} tcache_t;

#define TCACHE_N 700
static tcache_t tcache[TCACHE_N];
static int tcache_n;
static unsigned long tcache_tick;

static const char *famille(int style)
{
    /* Chicago n'existe pas : DejaVu Sans gras s'en approche le mieux */
    return style == TF_BOLD ? "DejaVu Sans" : "DejaVu Sans";
}

static tcache_t *tcache_get(const char *s, int size, int style)
{
    for (int i = 0; i < tcache_n; i++)
        if (tcache[i].size == size && tcache[i].style == style &&
            strcmp(tcache[i].txt, s) == 0) {
            tcache[i].used = ++tcache_tick;
            return &tcache[i];
        }

    /* rendu */
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *lay = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, famille(style));
    pango_font_description_set_absolute_size(fd, size * PANGO_SCALE);
    pango_font_description_set_weight(fd, style == TF_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(lay, fd);
    pango_layout_set_text(lay, s, -1);
    int pw, ph;
    pango_layout_get_pixel_size(lay, &pw, &ph);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    surf = cairo_image_surface_create(CAIRO_FORMAT_A8, pw + 2, ph + 2);
    cr = cairo_create(surf);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    /* A8 = le canal alpha sert d'encre : fond transparent, glyphes opaques */
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    lay = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(lay, fd);
    pango_layout_set_text(lay, s, -1);
    pango_cairo_update_layout(cr, lay);
    pango_cairo_show_layout(cr, lay);
    cairo_surface_flush(surf);
    g_object_unref(lay);
    pango_font_description_free(fd);

    tcache_t *e;
    if (tcache_n < TCACHE_N) {
        e = &tcache[tcache_n++];
    } else {
        /* evince l'entree la moins recemment utilisee */
        e = &tcache[0];
        for (int i = 1; i < TCACHE_N; i++)
            if (tcache[i].used < e->used) e = &tcache[i];
        free(e->mask); free(e->txt);
    }
    e->txt = strdup(s);
    e->size = size; e->style = style;
    e->w = pw + 2; e->h = ph + 2;
    e->mask = malloc((size_t)e->w * (size_t)e->h);
    e->used = ++tcache_tick;

    unsigned char *data = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);
    for (int j = 0; j < e->h; j++)
        for (int i = 0; i < e->w; i++)
            e->mask[(size_t)j * e->w + i] = data[(size_t)j * stride + i] >= 128 ? 1 : 0;

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return e;
}

int tx_width(const char *s, int size, int style)
{
    if (!s || !*s) return 0;
    return tcache_get(s, size, style)->w - 2;
}

int tx_height(int size, int style)
{
    return tcache_get("Ag", size, style)->h - 2;
}

void tx_draw(fb_t *f, int x, int y, const char *s, int size, int style)
{
    if (!s || !*s) return;
    tcache_t *e = tcache_get(s, size, style);
    for (int j = 0; j < e->h; j++)
        for (int i = 0; i < e->w; i++)
            if (e->mask[(size_t)j * e->w + i]) fb_pset(f, x + i - 1, y + j - 1, 1);
}

void tx_draw_inv(fb_t *f, int x, int y, const char *s, int size, int style)
{
    int w = tx_width(s, size, style) + 2, h = tx_height(size, style) + 2;
    fb_invert(f, x - 1, y - 1, w, h);
    tx_draw(f, x, y, s, size, style);
}

void tx_cache_clear(void)
{
    for (int i = 0; i < tcache_n; i++) { free(tcache[i].mask); free(tcache[i].txt); }
    tcache_n = 0;
}
