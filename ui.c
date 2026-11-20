/*  ui.c — la machine autour de l'ecran : boitier beige, lecteur de
 *  disquette, clavier et souris, dessines au cairo (pas de photo, pas
 *  d'image : tout est vectoriel).
 */
#include "oldmac.h"
#include <cairo/cairo.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define DW 1000.0
#define DH 1060.0

typedef struct {
    rect_t r;
    char   label[8];
    char   key;       /* caractere envoye, 0 = touche speciale */
    int    special;   /* 1 = maj, 2 = retour, 3 = espace, 4 = tab */
} key_t;

static key_t keys[80];
static int   keys_n;

/* disposition du clavier, en unites de dessin */
static void keys_layout(void)
{
    if (keys_n) return;
    keys_n = 0;
    struct { const char *l; char k; double x; double w; int sp; } r1[] = {
        { "esc", 0,  10, 34, 5 }, { "1", '1', 50, 30, 0 }, { "2", '2', 84, 30, 0 },
        { "3", '3', 118, 30, 0 }, { "4", '4', 152, 30, 0 }, { "5", '5', 186, 30, 0 },
        { "6", '6', 220, 30, 0 }, { "7", '7', 254, 30, 0 }, { "8", '8', 288, 30, 0 },
        { "9", '9', 322, 30, 0 }, { "0", '0', 356, 30, 0 }, { "-", '-', 390, 30, 0 },
        { "=", '=', 424, 30, 0 }, { "ret", 0, 458, 52, 2 },
    };
    for (size_t i = 0; i < sizeof(r1) / sizeof(r1[0]); i++) {
        key_t *k = &keys[keys_n++];
        k->r = (rect_t){ r1[i].x + 100, 706, r1[i].w, 30 };
        snprintf(k->label, sizeof(k->label), "%s", r1[i].l);
        k->key = r1[i].k; k->special = r1[i].sp;
    }
    struct { const char *l; char k; double x; double w; } r2[] = {
        { "tab", 0, 10, 44 }, { "Q", 'q', 60, 32 }, { "W", 'w', 96, 32 },
        { "E", 'e', 132, 32 }, { "R", 'r', 168, 32 }, { "T", 't', 204, 32 },
        { "Y", 'y', 240, 32 }, { "U", 'u', 276, 32 }, { "I", 'i', 312, 32 },
        { "O", 'o', 348, 32 }, { "P", 'p', 384, 32 }, { "[", '[', 420, 30 },
        { "]", ']', 454, 30 }, { "\\", '\\', 488, 22 },
    };
    for (size_t i = 0; i < sizeof(r2) / sizeof(r2[0]); i++) {
        key_t *k = &keys[keys_n++];
        k->r = (rect_t){ r2[i].x + 100, 742, r2[i].w, 30 };
        snprintf(k->label, sizeof(k->label), "%s", r2[i].l);
        k->key = r2[i].k; k->special = (i == 0) ? 4 : 0;
    }
    struct { const char *l; char k; double x; double w; } r3[] = {
        { "caps", 0, 10, 52 }, { "A", 'a', 68, 32 }, { "S", 's', 104, 32 },
        { "D", 'd', 140, 32 }, { "F", 'f', 184, 32 }, { "G", 'g', 220, 32 },
        { "H", 'h', 256, 32 }, { "J", 'j', 292, 32 }, { "K", 'k', 328, 32 },
        { "L", 'l', 364, 32 }, { ";", ';', 400, 30 }, { "'", '\'', 434, 30 },
        { "return", 0, 468, 42 },
    };
    for (size_t i = 0; i < sizeof(r3) / sizeof(r3[0]); i++) {
        key_t *k = &keys[keys_n++];
        k->r = (rect_t){ r3[i].x + 100, 778, r3[i].w, 30 };
        snprintf(k->label, sizeof(k->label), "%s", r3[i].l);
        k->key = r3[i].k; k->special = (i == 0) ? 1 : 0;
    }
    struct { const char *l; char k; double x; double w; } r4[] = {
        { "shift", 0, 10, 62 }, { "Z", 'z', 78, 32 }, { "X", 'x', 114, 32 },
        { "C", 'c', 150, 32 }, { "V", 'v', 186, 32 }, { "B", 'b', 222, 32 },
        { "N", 'n', 258, 32 }, { "M", 'm', 294, 32 }, { ",", ',', 330, 30 },
        { ".", '.', 364, 30 }, { "/", '/', 398, 30 }, { "shift", 0, 432, 78 },
    };
    for (size_t i = 0; i < sizeof(r4) / sizeof(r4[0]); i++) {
        key_t *k = &keys[keys_n++];
        k->r = (rect_t){ r4[i].x + 100, 814, r4[i].w, 30 };
        snprintf(k->label, sizeof(k->label), "%s", r4[i].l);
        k->key = r4[i].k; k->special = (i == 0 || i == 11) ? 1 : 0;
    }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 10, 850, 60, 30 };
      snprintf(k->label, sizeof(k->label), "alt"); k->key = 0; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 76, 850, 226, 30 };
      snprintf(k->label, sizeof(k->label), "espace"); k->key = ' '; k->special = 3; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 308, 850, 60, 30 };
      snprintf(k->label, sizeof(k->label), "alt"); k->key = 0; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 374, 850, 46, 30 };
      snprintf(k->label, sizeof(k->label), "left"); k->key = 0; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 426, 850, 46, 30 };
      snprintf(k->label, sizeof(k->label), "down"); k->key = 0; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 100 + 478, 850, 46, 30 };
      snprintf(k->label, sizeof(k->label), "right"); k->key = 0; k->special = 0; }

    /* pavé numérique, comme sur le clavier du Mac Plus */
    static const char *pad[3][4] = {
        { "7", "8", "9", "+" },
        { "4", "5", "6", "-" },
        { "1", "2", "3", "*" },
    };
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            key_t *k = &keys[keys_n++];
            k->r = (rect_t){ 620 + c * 42, 706 + r * 36, 38, 32 };
            snprintf(k->label, sizeof(k->label), "%s", pad[r][c]);
            k->key = pad[r][c][0];
            k->special = 0;
        }
    }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 620, 814, 80, 32 };
      snprintf(k->label, sizeof(k->label), "0"); k->key = '0'; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 704, 814, 38, 32 };
      snprintf(k->label, sizeof(k->label), "."); k->key = '.'; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 746, 814, 38, 32 };
      snprintf(k->label, sizeof(k->label), "="); k->key = '='; k->special = 0; }
    { key_t *k = &keys[keys_n++];
      k->r = (rect_t){ 620, 850, 164, 30 };
      snprintf(k->label, sizeof(k->label), "enter"); k->key = 0; k->special = 2; }
}

void ui_layout(chrome_t *c, double ww, double wh)
{
    keys_layout();
    c->w = ww; c->h = wh;
    double k = ww / DW;
    double ky = wh / DH;
    if (ky < k) k = ky;
    c->echelle = k;
    double ox = (ww - DW * k) / 2.0;
    double oy = (wh - DH * k) / 2.0;
    c->boitier = (rect_t){ ox + 120 * k, oy + 26 * k, 760 * k, 634 * k };
    c->ecran = (rect_t){ ox + 190 * k, oy + 96 * k, 620 * k, 620.0 * 342.0 / 512.0 * k };
    c->lecteur = (rect_t){ ox + 470 * k, oy + 588 * k, 170 * k, 16 * k };
    c->clavier = (rect_t){ ox + 100 * k, oy + 700 * k, 800 * k, 186 * k };
    c->souris = (rect_t){ ox + 730 * k, oy + 986 * k, 92 * k, 62 * k };
    /* on garde l'origine pour les elements dessines en coordonnees design */
    c->w = ww; c->h = wh;
}

static void ronde(cairo_t *cr, double x, double y, double w, double h, double r)
{
    double d = 2 * r;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
    (void)d;
}

void ui_map_window_to_screen(const chrome_t *c, double wx, double wy, int *sx, int *sy)
{
    double fx = (wx - c->ecran.x) / c->ecran.w;
    double fy = (wy - c->ecran.y) / c->ecran.h;
    *sx = (int)(fx * SCR_W);
    *sy = (int)(fy * SCR_H);
}

int ui_hit_key(const chrome_t *c, double wx, double wy, char *out)
{
    double k = c->echelle;
    double ox = (c->w - DW * k) / 2.0;
    double oy = (c->h - DH * k) / 2.0;
    for (int i = 0; i < keys_n; i++) {
        double x = ox + keys[i].r.x * k, y = oy + keys[i].r.y * k;
        double w = keys[i].r.w * k, h = keys[i].r.h * k;
        if (wx >= x && wx <= x + w && wy >= y && wy <= y + h) {
            if (out) *out = keys[i].key;
            return keys[i].special ? keys[i].special : (keys[i].key ? 10 : 0);
        }
    }
    return 0;
}

int ui_hit_mouse_button(const chrome_t *c, double wx, double wy)
{
    return (wx >= c->souris.x && wx <= c->souris.x + c->souris.w * 0.55 &&
            wy >= c->souris.y && wy <= c->souris.y + c->souris.h * 0.55);
}

/* ------------------------------------------------------------------ */
void ui_draw(cairo_t *cr, const chrome_t *c, fb_t *ecran, sys_t *s, int64_t now_ms)
{
    (void)now_ms;
    double k = c->echelle;
    double ox = (c->w - DW * k) / 2.0;
    double oy = (c->h - DH * k) / 2.0;

    /* fond : le bureau clair de la photo */
    cairo_pattern_t *bg = cairo_pattern_create_linear(0, 0, 0, c->h);
    cairo_pattern_add_color_stop_rgb(bg, 0, 0.96, 0.95, 0.93);
    cairo_pattern_add_color_stop_rgb(bg, 1, 0.88, 0.87, 0.85);
    cairo_set_source(cr, bg);
    cairo_paint(cr);
    cairo_pattern_destroy(bg);

    /* ombre portee du boitier */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.16);
    ronde(cr, c->boitier.x + 8, c->boitier.y + 14, c->boitier.w, c->boitier.h, 26 * k);
    cairo_fill(cr);

    /* boitier beige : degrade vertical leger */
    cairo_pattern_t *beige = cairo_pattern_create_linear(0, c->boitier.y, 0,
                                                        c->boitier.y + c->boitier.h);
    cairo_pattern_add_color_stop_rgb(beige, 0, 0.91, 0.88, 0.79);
    cairo_pattern_add_color_stop_rgb(beige, 0.55, 0.87, 0.84, 0.74);
    cairo_pattern_add_color_stop_rgb(beige, 1, 0.80, 0.77, 0.67);
    cairo_set_source(cr, beige);
    ronde(cr, c->boitier.x, c->boitier.y, c->boitier.w, c->boitier.h, 24 * k);
    cairo_fill(cr);
    cairo_pattern_destroy(beige);

    /* chanfrein clair en haut, ombre fine en bas */
    cairo_set_source_rgba(cr, 1, 1, 1, 0.55);
    cairo_set_line_width(cr, 1.6 * k);
    cairo_arc(cr, c->boitier.x + 24 * k, c->boitier.y + 24 * k, 24 * k, M_PI, 1.5 * M_PI);
    cairo_arc(cr, c->boitier.x + c->boitier.w - 24 * k, c->boitier.y + 24 * k, 24 * k,
              1.5 * M_PI, 2 * M_PI);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.10);
    cairo_set_line_width(cr, 1.2 * k);
    cairo_arc(cr, c->boitier.x + 24 * k, c->boitier.y + c->boitier.h - 24 * k, 24 * k,
              M_PI / 2, M_PI);
    cairo_arc(cr, c->boitier.x + c->boitier.w - 24 * k, c->boitier.y + c->boitier.h - 24 * k,
              24 * k, 0, M_PI / 2);
    cairo_stroke(cr);

    /* creux de l'ecran : bezel sombre arrondi */
    double bx = ox + 160 * k, by = oy + 66 * k;
    double bw = 680 * k, bh = 500 * k;
    cairo_pattern_t *bez = cairo_pattern_create_linear(0, by, 0, by + bh);
    cairo_pattern_add_color_stop_rgb(bez, 0, 0.62, 0.61, 0.57);
    cairo_pattern_add_color_stop_rgb(bez, 0.12, 0.42, 0.41, 0.38);
    cairo_pattern_add_color_stop_rgb(bez, 1, 0.34, 0.33, 0.31);
    cairo_set_source(cr, bez);
    ronde(cr, bx, by, bw, bh, 14 * k);
    cairo_fill(cr);
    cairo_pattern_destroy(bez);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    ronde(cr, bx, by, bw, bh, 14 * k);
    cairo_set_line_width(cr, 1.4 * k);
    cairo_stroke(cr);

    /* la dalle : 512 x 342 pixels, chacun un petit carre */
    double sx = c->ecran.x, sy = c->ecran.y;
    double px = c->ecran.w / SCR_W, py = c->ecran.h / SCR_H;
    double lum = 0.35 + 0.65 * (s->bright / 100.0);

    /* ecran eteint : verre noir */
    if (!s->ecran_allume) {
        cairo_set_source_rgb(cr, 0.05, 0.05, 0.05);
        cairo_rectangle(cr, sx, sy, c->ecran.w, c->ecran.h);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.05);
        cairo_rectangle(cr, sx + 10, sy + 6, c->ecran.w - 40, 2);
        cairo_fill(cr);
    } else {
        cairo_set_source_rgb(cr, 0.93 * lum + 0.07, 1.00 * lum + 0.05, 0.99 * lum + 0.05);
        cairo_rectangle(cr, sx, sy, c->ecran.w, c->ecran.h);
        cairo_fill(cr);
        /* l'encre : le framebuffer passe par un masque, agrandi au pixel le
           plus proche — aucun flou gris, des pixels francs comme sur un tube */
        cairo_surface_t *masque = cairo_image_surface_create(CAIRO_FORMAT_A8, SCR_W, SCR_H);
        unsigned char *md = cairo_image_surface_get_data(masque);
        int ms = cairo_image_surface_get_stride(masque);
        for (int y = 0; y < SCR_H; y++) {
            unsigned char *l = md + (size_t)y * ms;
            const uint8_t *src = ecran->p + (size_t)y * ecran->stride;
            for (int x = 0; x < SCR_W; x++) l[x] = src[x] ? 255 : 0;
        }
        cairo_surface_mark_dirty(masque);
        cairo_pattern_t *encre = cairo_pattern_create_for_surface(masque);
        cairo_pattern_set_filter(encre, CAIRO_FILTER_NEAREST);
        cairo_save(cr);
        cairo_translate(cr, sx, sy);
        cairo_scale(cr, px, py);
        cairo_set_source_rgb(cr, 0.02, 0.03, 0.04);
        cairo_mask(cr, encre);
        cairo_restore(cr);
        cairo_pattern_destroy(encre);
        cairo_surface_destroy(masque);
        /* lignes de balayage discretes */
        cairo_set_source_rgba(cr, 0, 0, 0, 0.03);
        cairo_set_line_width(cr, py * 0.30);
        for (int y = 0; y < SCR_H; y += 2) {
            cairo_move_to(cr, sx, sy + y * py);
            cairo_line_to(cr, sx + c->ecran.w, sy + y * py);
        }
        cairo_stroke(cr);
    }

    /* reflet du verre */
    cairo_save(cr);
    ronde(cr, sx, sy, c->ecran.w, c->ecran.h, 3 * k);
    cairo_clip(cr);
    cairo_pattern_t *refl = cairo_pattern_create_linear(sx, sy, sx + c->ecran.w * 0.7,
                                                        sy + c->ecran.h);
    cairo_pattern_add_color_stop_rgba(refl, 0, 1, 1, 1, 0.10);
    cairo_pattern_add_color_stop_rgba(refl, 0.45, 1, 1, 1, 0.0);
    cairo_set_source(cr, refl);
    cairo_paint(cr);
    cairo_pattern_destroy(refl);
    cairo_restore(cr);

    /* le lecteur de disquette */
    cairo_set_source_rgb(cr, 0.28, 0.27, 0.25);
    ronde(cr, c->lecteur.x, c->lecteur.y, c->lecteur.w, c->lecteur.h, 2 * k);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    cairo_rectangle(cr, c->lecteur.x + 3 * k, c->lecteur.y + c->lecteur.h * 0.35,
                    c->lecteur.w - 6 * k, 2.2 * k);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.25);
    cairo_rectangle(cr, c->lecteur.x + 3 * k, c->lecteur.y + 1 * k,
                    c->lecteur.w - 6 * k, 1.2 * k);
    cairo_fill(cr);

    /* badge pomme arc-en-ciel, comme sur la photo */
    double abx = ox + 198 * k, aby = oy + 596 * k, abs = 9 * k;
    cairo_set_source_rgb(cr, 0.55, 0.50, 0.42);
    cairo_arc(cr, abx + abs * 0.5, aby + abs * 0.55, abs * 0.62, 0, 2 * M_PI);
    cairo_fill(cr);
    const double couleurs[6][3] = {
        { 0.35, 0.62, 0.31 }, { 0.96, 0.78, 0.20 }, { 0.92, 0.45, 0.16 },
        { 0.80, 0.19, 0.18 }, { 0.36, 0.30, 0.66 }, { 0.30, 0.63, 0.86 },
    };
    for (int i = 0; i < 6; i++) {
        cairo_set_source_rgb(cr, couleurs[i][0], couleurs[i][1], couleurs[i][2]);
        cairo_rectangle(cr, abx, aby + abs * i / 6.0, abs, abs / 6.0 + 0.4);
        cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, 0.86, 0.84, 0.75);
    cairo_rectangle(cr, abx - abs * 0.12, aby - abs * 0.30, abs * 1.24, abs * 0.30);
    cairo_fill(cr);

    /* bouton de luminosite, en bas a droite */
    double kbx = ox + 812 * k, kby = oy + 600 * k;
    cairo_set_source_rgb(cr, 0.72, 0.69, 0.60);
    cairo_arc(cr, kbx, kby, 11 * k, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
    cairo_set_line_width(cr, 1.2 * k);
    cairo_arc(cr, kbx, kby, 11 * k, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.55, 0.52, 0.45);
    cairo_rectangle(cr, kbx - 1.2 * k, kby - 7 * k, 2.4 * k, 6 * k);
    cairo_fill(cr);

    /* la diode de facade */
    cairo_set_source_rgba(cr, 0.35, 0.75, 0.45, s->ecran_allume ? 0.95 : 0.25);
    cairo_arc(cr, ox + 760 * k, oy + 600 * k, 2.2 * k, 0, 2 * M_PI);
    cairo_fill(cr);

    /* fentes d'aeration sous le boitier */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.10);
    for (int i = 0; i < 10; i++) {
        cairo_rectangle(cr, ox + (200 + i * 12) * k, oy + 646 * k, 8 * k, 2 * k);
        cairo_fill(cr);
    }

    /* ---------------------------------------------------------------- */
    /* clavier                                                          */
    /* ---------------------------------------------------------------- */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.14);
    ronde(cr, c->clavier.x + 6, c->clavier.y + 8, c->clavier.w, c->clavier.h, 8 * k);
    cairo_fill(cr);
    cairo_pattern_t *kb = cairo_pattern_create_linear(0, c->clavier.y, 0,
                                                     c->clavier.y + c->clavier.h);
    cairo_pattern_add_color_stop_rgb(kb, 0, 0.89, 0.87, 0.79);
    cairo_pattern_add_color_stop_rgb(kb, 1, 0.79, 0.76, 0.66);
    cairo_set_source(cr, kb);
    ronde(cr, c->clavier.x, c->clavier.y, c->clavier.w, c->clavier.h, 7 * k);
    cairo_fill(cr);
    cairo_pattern_destroy(kb);

    for (int i = 0; i < keys_n; i++) {
        double x = ox + keys[i].r.x * k, y = oy + keys[i].r.y * k;
        double w = keys[i].r.w * k, h = keys[i].r.h * k;
        cairo_set_source_rgba(cr, 0, 0, 0, 0.18);
        ronde(cr, x + 1.5 * k, y + 2.2 * k, w - 1 * k, h - 1 * k, 3 * k);
        cairo_fill(cr);
        cairo_pattern_t *kt = cairo_pattern_create_linear(0, y, 0, y + h);
        cairo_pattern_add_color_stop_rgb(kt, 0, 0.83, 0.81, 0.73);
        cairo_pattern_add_color_stop_rgb(kt, 1, 0.70, 0.67, 0.58);
        cairo_set_source(cr, kt);
        ronde(cr, x, y, w, h - 1.5 * k, 3 * k);
        cairo_fill(cr);
        cairo_pattern_destroy(kt);
        cairo_set_source_rgba(cr, 0.45, 0.42, 0.36, 0.75);
        cairo_select_font_face(cr, "DejaVu Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 6.4 * k < 5 ? 5 : 6.4 * k);
        cairo_text_extents_t te;
        cairo_text_extents(cr, keys[i].label, &te);
        cairo_move_to(cr, x + (w - te.width) / 2, y + h / 2 + te.height / 2);
        cairo_show_text(cr, keys[i].label);
    }

    /* ---------------------------------------------------------------- */
    /* souris                                                           */
    /* ---------------------------------------------------------------- */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.12);
    ronde(cr, c->souris.x + 4, c->souris.y + 6, c->souris.w, c->souris.h, 14 * k);
    cairo_fill(cr);
    cairo_pattern_t *ms = cairo_pattern_create_linear(0, c->souris.y, 0,
                                                     c->souris.y + c->souris.h);
    cairo_pattern_add_color_stop_rgb(ms, 0, 0.90, 0.88, 0.80);
    cairo_pattern_add_color_stop_rgb(ms, 1, 0.76, 0.73, 0.63);
    cairo_set_source(cr, ms);
    ronde(cr, c->souris.x, c->souris.y, c->souris.w, c->souris.h, 14 * k);
    cairo_fill(cr);
    cairo_pattern_destroy(ms);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.16);
    cairo_move_to(cr, c->souris.x + 12 * k, c->souris.y + 4 * k);
    cairo_line_to(cr, c->souris.x + c->souris.w - 12 * k, c->souris.y + 4 * k);
    cairo_stroke(cr);

    /* cable de la souris vers le clavier, en passant sous le clavier */
    cairo_set_source_rgba(cr, 0.62, 0.60, 0.52, 0.9);
    cairo_set_line_width(cr, 2.0 * k);
    cairo_move_to(cr, c->souris.x, c->souris.y + c->souris.h * 0.5);
    cairo_curve_to(cr, c->souris.x - 70 * k, c->souris.y + 40 * k,
                   c->clavier.x + c->clavier.w - 40 * k,
                   c->clavier.y + c->clavier.h + 60 * k,
                   c->clavier.x + c->clavier.w - 40 * k,
                   c->clavier.y + c->clavier.h - 4 * k);
    cairo_stroke(cr);
}
