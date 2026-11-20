/*  apps.c — le Systeme simule : bureau, barre de menus, fenetres,
 *  Finder, MacWrite, MacPaint, tableau de bord, dialogues.
 *
 *  Ecran 512 x 342, 1 bit, comme le Macintosh Plus. Tout est dessine
 *  a la main : aucune image, aucun theme, aucune dependance hors GTK.
 */
#include "oldmac.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>

#define MAX_TXT  16384

sys_t G;

/* ------------------------------------------------------------------ */
/* etats des applications                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    char text[MAX_TXT];
    int  len, caret, scroll, dirty;
    char name[40];
} write_st;

typedef struct {
    uint8_t bits[440 * 240];
    int  w, h;
    int  tool, down, lx, ly;
    char name[40];
    int  dirty;
} paint_st;

typedef struct {
    int sel;
    int tri;
} finder_st;

enum { TOOL_CRAYON = 0, TOOL_GOMME, TOOL_LIGNE, TOOL_CADRE, TOOL_POT };

static char clip[8192];
static int  clip_n;

static char alerte_txt[200];
static const char *alert_lines[4];
static int alert_n;

/* icones du bureau (position en coordonnees ecran) */
typedef struct { int x, y, kind; char name[40]; } icon_t;
static icon_t icons[16];
static int icon_n;
static int icon_sel = -1;

static int64_t now_ms_cache;
static int64_t t0_ms;

/* ------------------------------------------------------------------ */
static int64_t ms_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void sys_init(sys_t *s)
{
    memset(s, 0, sizeof(*s));
    s->horloge = 1;
    s->ecran_allume = 1;
    s->bright = 92;
    s->son = 60;
    s->annee = 1986;
    s->boot_phase = 0;
    s->menu.open = -1;
    s->menu.item = -1;
    s->curseur_x = 256;
    s->curseur_y = 180;
    s->curseur_lu = 1;
    s->statut_ms = 2200;
    snprintf(s->statut, sizeof(s->statut), "Bienvenue sur ce Macintosh");
    t0_ms = ms_now();
    s->boot_ms = t0_ms;

    gethostname(s->host, sizeof(s->host));
    struct utsname u;
    if (uname(&u) == 0)
        snprintf(s->kernel, sizeof(s->kernel), "%.40s %.48s", u.sysname, u.release);
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f))
            if (sscanf(line, "MemTotal: %lu kB", &s->ram_ko) == 1) break;
        fclose(f);
    }
    f = fopen("/proc/uptime", "r");
    if (f) { if (fscanf(f, "%lf", &s->uptime_s) != 1) s->uptime_s = 0; fclose(f); }
    f = fopen("/proc/loadavg", "r");
    if (f) { if (fscanf(f, "%lf", &s->load1) != 1) s->load1 = 0; fclose(f); }

    disk_init(&s->disk);
    wm_init(&s->wm);
    icon_n = 0;
    icons[icon_n++] = (icon_t){ 30, 46, -1, "Macintosh HD" };
    icons[icon_n++] = (icon_t){ 30, 126, APP_WRITE, "MacWrite" };
    icons[icon_n++] = (icon_t){ 30, 206, APP_PAINT, "MacPaint" };
    icons[icon_n++] = (icon_t){ 30, 278, APP_TEL, "BATMino" };
    icons[icon_n++] = (icon_t){ 440, 250, -2, "Corbeille" };

    /* la telecommande de la carte : le port est attribue, pas ouvert */
    tel_init(&G_TEL, TEL_PORT_DEFAUT);
}

/* ------------------------------------------------------------------ */
/* petits dessins                                                     */
/* ------------------------------------------------------------------ */
static void pomme(fb_t *f, int x, int y)
{
    /* corps : deux bosses, ventre arrondi, une morsure a droite */
    fb_circle(f, x + 4, y + 5, 4, 1);
    fb_circle(f, x + 9, y + 5, 4, 1);
    fb_rect(f, x + 1, y + 6, 12, 5, 1);
    fb_circle(f, x + 4, y + 10, 3, 1);
    fb_circle(f, x + 9, y + 10, 3, 1);
    fb_circle(f, x + 13, y + 7, 3, 0);
    /* la feuille */
    fb_line(f, x + 7, y + 1, x + 10, y - 2, 1);
    fb_line(f, x + 8, y + 1, x + 11, y - 2, 1);
}

static void icone_disque(fb_t *f, int x, int y)
{
    fb_rrect(f, x + 2, y + 2, 28, 24, 2, 1);
    fb_rrect(f, x + 4, y + 4, 24, 20, 1, 0);
    fb_rect(f, x + 6, y + 19, 20, 3, 1);
    fb_rect(f, x + 22, y + 21, 3, 1, 0);
    fb_hline(f, x + 6, y + 8, 14, 1);
    fb_hline(f, x + 6, y + 10, 10, 1);
    /* ombre portee */
    for (int i = 0; i < 26; i++) fb_pset(f, x + 4 + i, y + 27, 1);
    for (int i = 0; i < 24; i++) fb_pset(f, x + 30, y + 4 + i, 1);
}

static void icone_doc(fb_t *f, int x, int y)
{
    fb_rect(f, x + 5, y + 2, 22, 26, 1);
    fb_rect(f, x + 7, y + 4, 18, 22, 0);
    fb_rect(f, x + 21, y + 2, 6, 6, 0);
    fb_line(f, x + 20, y + 3, x + 26, y + 9, 1);
    for (int i = 0; i < 4; i++) fb_hline(f, x + 9, y + 10 + i * 3, 12 - i * 2, 1);
    for (int i = 0; i < 26; i++) fb_pset(f, x + 28, y + 3 + i, 1);
}

static void icone_pinceau(fb_t *f, int x, int y)
{
    icone_doc(f, x, y);
    /* un pinceau en travers du document */
    fb_line(f, x + 9, y + 22, x + 20, y + 11, 1);
    fb_line(f, x + 10, y + 23, x + 21, y + 12, 1);
    fb_circle(f, x + 22, y + 10, 2, 1);
    fb_pset(f, x + 8, y + 24, 1);
}

static void icone_corbeille(fb_t *f, int x, int y)
{
    fb_rect(f, x + 8, y + 6, 17, 22, 1);
    fb_rect(f, x + 9, y + 7, 15, 20, 0);
    fb_hline(f, x + 6, y + 5, 21, 1);
    fb_hline(f, x + 11, y + 3, 11, 1);
    fb_vline(f, x + 12, y + 9, 16, 1);
    fb_vline(f, x + 16, y + 9, 16, 1);
    fb_vline(f, x + 20, y + 9, 16, 1);
    for (int i = 0; i < 22; i++) fb_pset(f, x + 26, y + 7 + i, 1);
}

static void icone_carte(fb_t *f, int x, int y)
{
    /* la carte e-paper : boitier, dalle claire, deux touches, l'ombre */
    fb_rrect(f, x + 2, y + 2, 28, 22, 2, 1);
    fb_rect(f, x + 5, y + 5, 22, 13, 0);
    fb_frame(f, x + 5, y + 5, 22, 13);
    fb_rect(f, x + 8, y + 8, 15, 2, 1);
    fb_rect(f, x + 8, y + 12, 9, 2, 1);
    fb_rect(f, x + 8, y + 20, 6, 2, 0);
    fb_rect(f, x + 18, y + 20, 6, 2, 0);
    for (int i = 0; i < 26; i++) fb_pset(f, x + 4 + i, y + 25, 1);
    for (int i = 0; i < 22; i++) fb_pset(f, x + 30, y + 4 + i, 1);
}

static void icone_app(fb_t *f, int kind, int x, int y)
{
    switch (kind) {
    case APP_WRITE: icone_doc(f, x, y); break;
    case APP_PAINT: icone_pinceau(f, x, y); break;
    case APP_TEL:   icone_carte(f, x, y); break;
    case -2:        icone_corbeille(f, x, y); break;
    default:        icone_disque(f, x, y); break;
    }
}

static void curseur(fb_t *f, int x, int y)
{
    for (int i = 0; i < 12; i++) {
        fb_line(f, x, y + i, x + i / 2, y + i, 1);
    }
    fb_line(f, x, y, x, y + 11, 1);
    fb_line(f, x + 1, y + 1, x + 5, y + 6, 1);
    fb_line(f, x + 4, y + 9, x + 8, y + 12, 1);
    fb_line(f, x + 5, y + 9, x + 9, y + 12, 1);
    fb_pset(f, x + 9, y + 13, 1);
    /* contour blanc d'un pixel pour rester lisible sur le noir */
    for (int i = 1; i < 11; i++) { fb_pset(f, x + 1, y + i, 0); }
}

/* ------------------------------------------------------------------ */
/* barre de menus                                                     */
/* ------------------------------------------------------------------ */
static int menu_x[5];      /* position du titre dans la barre */
static int menu_w[5];

static void barre_menus(fb_t *f, sys_t *s)
{
    fb_rect(f, 0, 0, SCR_W, MB_H, 0);
    fb_hline(f, 0, MB_H - 1, SCR_W, 1);
    pomme(f, 8, 4);
    /* la pomme est un menu a elle seule, puis les cinq titres */
    menu_x[0] = 3; menu_w[0] = 19;
    int x = 24;
    for (int i = 1; i < 5; i++) {
        const char *t = MENU_TITRES[i];
        int tw = tx_width(t, 12, TF_BOLD);
        menu_x[i] = x;
        menu_w[i] = tw + 16;
        tx_draw(f, x + 7, 3, t, 12, TF_BOLD);
        x += menu_w[i];
    }

    /* horloge a droite, a l'heure de la machine */
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char h[16];
    if (s->horloge) {
        snprintf(h, sizeof(h), "%d:%02d", tm.tm_hour, tm.tm_min);
        int w = tx_width(h, 12, TF_BOLD);
        tx_draw(f, SCR_W - w - 8, 3, h, 12, TF_BOLD);
    }
}

/* ------------------------------------------------------------------ */
/* fenetres                                                           */
/* ------------------------------------------------------------------ */
static void barre_titre(fb_t *f, win_t *w)
{
    fb_frame(f, w->x, w->y, w->w, w->h);
    fb_rect(f, w->x + 1, w->y + 1, w->w - 2, TITLE_H, 0);
    if (w->active) {
        for (int j = 3; j < TITLE_H - 2; j += 2)
            fb_hline(f, w->x + 15, w->y + j, w->w - 19, 1);
    } else {
        fb_hline(f, w->x + 15, w->y + TITLE_H - 2, w->w - 19, 1);
    }
    /* case de fermeture */
    fb_rect(f, w->x + 4, w->y + 4, 11, 11, 0);
    fb_frame(f, w->x + 4, w->y + 4, 11, 11);
    /* titre : efface la trame derriere le texte */
    int tw = tx_width(w->title, 11, TF_BOLD);
    int tx = w->x + (w->w - tw) / 2;
    if (w->active) fb_rect(f, tx - 3, w->y + 3, tw + 6, 13, 0);
    tx_draw(f, tx, w->y + 3, w->title, 11, TF_BOLD);
    /* case de zoom */
    fb_frame(f, w->x + w->w - 15, w->y + 4, 11, 11);
    fb_hline(f, w->x + w->w - 13, w->y + 12, 7, 1);
}

/* --- Finder ------------------------------------------------------- */
static void draw_finder(fb_t *f, win_t *w, sys_t *s)
{
    finder_st *st = (finder_st *)w->st;
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int cw = w->w - 2, ch = w->h - TITLE_H - 2;
    fb_rect(f, x, y, cw, ch, 0);
    int row = 16;
    int n = s->disk.n + 1;
    int visibles = ch / row - 1;
    fb_rect(f, x, y, cw, 12, 0);
    char entete[80];
    snprintf(entete, sizeof(entete), "%d elements, %d Ko libres", n, disk_free_ko());
    tx_draw(f, x + 4, y + 1, entete, 10, TF_NORMAL);
    fb_hline(f, x, y + 12, cw, 1);

    int premier = st ? st->sel - (st->sel >= visibles ? st->sel - visibles + 1 : 0) : 0;
    for (int i = 0; i < visibles; i++) {
        int k = premier + i;
        if (k >= n) break;
        int iy = y + 14 + i * row;
        const char *nom;
        int kind;
        if (k == 0) { nom = "Macintosh HD"; kind = -1; }
        else { nom = s->disk.f[k - 1].name; kind = s->disk.f[k - 1].type == FT_TEXT ? APP_WRITE : APP_PAINT; }
        if (st && st->sel == k) fb_rect(f, x + 1, iy - 1, cw - 2, row - 2, 1);
        int inv = st && st->sel == k;
        if (inv) {
            tx_draw_inv(f, x + 24, iy + 2, nom, 11, TF_BOLD);
        } else {
            tx_draw(f, x + 24, iy + 2, nom, 11, TF_BOLD);
        }
        if (kind == -1) icone_disque(f, x + 3, iy - 3);
        else if (kind == APP_WRITE) icone_doc(f, x + 3, iy - 3);
        else icone_pinceau(f, x + 3, iy - 3);
    }
    /* barre de defilement simplifiee a droite */
    fb_vline(f, w->x + w->w - 14, y + 14, ch - 15, 1);
    fb_rect(f, w->x + w->w - 13, y + 16, 12, 20, 1);
}

/* --- MacWrite ----------------------------------------------------- */
static int  caret_on;
static void draw_write(fb_t *f, win_t *w, sys_t *s)
{
    (void)s;
    write_st *st = (write_st *)w->st;
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int cw = w->w - 2, ch = w->h - TITLE_H - 2;
    fb_rect(f, x, y, cw, ch, 0);
    fb_hline(f, x, y, cw, 1);

    int lh = 14, marge = 6;
    int lignes = (ch - 36) / lh;
    int col = 0, ligne = 0, caret_l = 0, caret_c = 0;
    int i = 0;
    int start_line = st->scroll;
    for (i = 0; i < st->len; i++) {
        char c = st->text[i];
        if (i == st->caret) { caret_l = ligne; caret_c = col; }
        if (c == '\n') { ligne++; col = 0; continue; }
        col++;
        if (col > (cw - 2 * marge) / 7) { ligne++; col = 0; }
    }
    if (st->caret >= st->len) { caret_l = ligne; caret_c = col; }

    /* dessin caractere par caractere, regroupes par ligne */
    col = 0; ligne = 0;
    char buf[128];
    int bi = 0;
    for (i = 0; i <= st->len; i++) {
        char c = (i < st->len) ? st->text[i] : '\n';
        if (c == '\n' || col >= (cw - 2 * marge) / 7) {
            buf[bi] = 0;
            if (ligne >= start_line && ligne - start_line < lignes)
                tx_draw(f, x + marge, y + 8 + (ligne - start_line) * lh, buf, 11, TF_NORMAL);
            bi = 0; ligne++; col = 0;
            if (c == '\n') continue;
        }
        if (c >= 32) buf[bi++] = c;
        col++;
    }
    /* curseur clignotant */
    if (st->dirty == 0 || 1) {
        if (caret_on && caret_l >= start_line && caret_l - start_line < lignes) {
            int cx = x + marge + caret_c * 7;
            int cy = y + 8 + (caret_l - start_line) * lh;
            fb_rect(f, cx, cy - 1, 1, 12, 1);
        }
    }
    /* pied : nom du document */
    fb_hline(f, x, y + ch - 14, cw, 1);
    char pied[64];
    snprintf(pied, sizeof(pied), "%.24s %d car.%s", st->name, st->len, st->dirty ? " (modifie)" : "");
    tx_draw(f, x + 6, y + ch - 12, pied, 10, TF_NORMAL);
}

/* --- MacPaint ----------------------------------------------------- */
#define PAINT_PAL 44

static void outil_glyphe(fb_t *f, int tool, int x, int y, int ink)
{
    switch (tool) {
    case TOOL_CRAYON:
        fb_line(f, x + 3, y + 16, x + 14, y + 5, ink);
        fb_line(f, x + 4, y + 17, x + 15, y + 6, ink);
        fb_rect(f, x + 2, y + 16, 3, 3, ink);
        break;
    case TOOL_GOMME:
        fb_rect(f, x + 3, y + 7, 13, 9, ink);
        fb_rect(f, x + 5, y + 9, 9, 5, ink ? 0 : 1);
        break;
    case TOOL_LIGNE:
        fb_line(f, x + 4, y + 16, x + 15, y + 4, ink);
        fb_pset(f, x + 4, y + 16, ink);
        fb_pset(f, x + 15, y + 4, ink);
        break;
    case TOOL_CADRE:
        fb_frame(f, x + 4, y + 5, 12, 11);
        break;
    default: { /* pot de peinture */
        fb_line(f, x + 5, y + 16, x + 13, y + 16, ink);
        fb_line(f, x + 5, y + 16, x + 9, y + 4, ink);
        fb_line(f, x + 13, y + 16, x + 9, y + 4, ink);
        for (int j = 6; j < 16; j++) {
            int d = (j - 6) * 4 / 10;
            fb_hline(f, x + 9 - d, y + j, 2 * d + 1, ink);
        }
        fb_pset(f, x + 16, y + 4, ink);
        break;
    }
    }
}

static void draw_paint(fb_t *f, win_t *w)
{
    paint_st *st = (paint_st *)w->st;
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int ch = w->h - TITLE_H - 2;
    fb_rect(f, x, y, w->w - 2, ch, 0);

    /* palette d'outils */
    fb_frame(f, x + 2, y + 2, PAINT_PAL, ch - 4);
    for (int i = 0; i < 5; i++) {
        int oy = y + 5 + i * 26;
        fb_frame(f, x + 5, oy, 24, 24);
        if (st->tool == i) fb_rect(f, x + 6, oy + 1, 22, 22, 1);
        outil_glyphe(f, i, x + 6, oy + 1, st->tool == i ? 0 : 1);
    }
    int my = y + 5 + 5 * 26 + 4;
    fb_frame(f, x + 5, my, 24, 22); fb_gray(f, x + 6, my + 1, 22, 20);
    fb_frame(f, x + 5, my + 26, 24, 22); fb_darkgray(f, x + 6, my + 27, 22, 20);

    /* zone de dessin */
    int cx = x + PAINT_PAL + 3, cy = y + 2;
    int cw = w->w - PAINT_PAL - 6, chh = ch - 20;
    fb_frame(f, cx, cy, cw, chh);
    for (int j = 0; j < chh - 2; j++)
        for (int i = 0; i < cw - 2; i++) {
            int gx = i, gy = j;
            if (gx < st->w && gy < st->h)
                fb_pset(f, cx + 1 + i, cy + 1 + j,
                        st->bits[(size_t)gy * st->w + gx]);
        }
    fb_hline(f, x, y + ch - 16, w->w - 2, 1);
    char pied[80];
    snprintf(pied, sizeof(pied), "%.22s  %dx%d%s", st->name, st->w, st->h,
             st->dirty ? " *" : "");
    tx_draw(f, x + PAINT_PAL + 6, y + ch - 13, pied, 10, TF_NORMAL);
    tx_draw(f, x + 6, y + ch - 13, "outils", 9, TF_NORMAL);
}

/* --- Tableau de bord ---------------------------------------------- */
static void draw_control(fb_t *f, win_t *w, sys_t *s)
{
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int cw = w->w - 2;
    fb_rect(f, x, y, cw, w->h - TITLE_H - 2, 0);
    fb_frame(f, x + 8, y + 6, cw - 16, 40);
    tx_draw(f, x + 14, y + 10, "Luminosite du tube", 10, TF_BOLD);
    fb_rect(f, x + 12, y + 26, cw - 24, 8, 0);
    fb_frame(f, x + 12, y + 26, cw - 24, 8);
    int bw = (cw - 38) * s->bright / 100;
    fb_rect(f, x + 13, y + 27, bw, 6, 1);
    fb_rect(f, x + 13 + bw, y + 25, 7, 10, 1);

    fb_frame(f, x + 8, y + 50, cw - 16, 40);
    tx_draw(f, x + 14, y + 54, "Volume du haut-parleur", 10, TF_BOLD);
    fb_rect(f, x + 12, y + 70, cw - 24, 8, 0);
    fb_frame(f, x + 12, y + 70, cw - 24, 8);
    int sv = (cw - 38) * s->son / 100;
    fb_rect(f, x + 13, y + 71, sv, 6, 1);
    fb_rect(f, x + 13 + sv, y + 69, 7, 10, 1);

    /* ce que la machine est vraiment */
    int iy = y + 98;
    fb_hline(f, x + 8, iy, cw - 16, 1);
    tx_draw(f, x + 14, iy + 4, "Cette machine, pour de vrai", 10, TF_BOLD);
    char l[128];
    snprintf(l, sizeof(l), "hote : %s", s->host);
    tx_draw(f, x + 14, iy + 20, l, 10, TF_NORMAL);
    snprintf(l, sizeof(l), "noyau : %.44s", s->kernel);
    tx_draw(f, x + 14, iy + 32, l, 10, TF_NORMAL);
    snprintf(l, sizeof(l), "memoire : %lu Mo | charge : %.2f", s->ram_ko / 1024, s->load1);
    tx_draw(f, x + 14, iy + 44, l, 10, TF_NORMAL);
    snprintf(l, sizeof(l), "en marche depuis %d min", (int)(s->uptime_s / 60));
    tx_draw(f, x + 14, iy + 56, l, 10, TF_NORMAL);

    /* le Mac simule */
    fb_hline(f, x + 8, iy + 70, cw - 16, 1);
    tx_draw(f, x + 14, iy + 74, "Ce Macintosh simule", 10, TF_BOLD);
    snprintf(l, sizeof(l), "Systeme 6.0.8 | 1024 Ko de RAM | 512x342");
    tx_draw(f, x + 14, iy + 90, l, 10, TF_NORMAL);
    snprintf(l, sizeof(l), "disque : %d Ko utilises sur %d", disk_used_ko(), disk_capacity_ko());
    tx_draw(f, x + 14, iy + 102, l, 10, TF_NORMAL);
    snprintf(l, sizeof(l), "%d fenetre(s) ouverte(s) | oldmac %s", s->wm.n, VERSION_STR);
    tx_draw(f, x + 14, iy + 114, l, 10, TF_NORMAL);
}

/* --- A propos ----------------------------------------------------- */
static void draw_about(fb_t *f, win_t *w)
{
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int cw = w->w - 2;
    fb_rect(f, x, y, cw, w->h - TITLE_H - 2, 0);
    /* le Mac en miniature */
    fb_rrect(f, x + 14, y + 12, 62, 52, 3, 1);
    fb_rect(f, x + 22, y + 20, 46, 32, 0);
    fb_frame(f, x + 22, y + 20, 46, 32);
    fb_rect(f, x + 40, y + 56, 14, 3, 1);
    fb_circle(f, x + 24, y + 60, 2, 1);

    tx_draw(f, x + 88, y + 12, "Macintosh Plus", 13, TF_BOLD);
    tx_draw(f, x + 88, y + 32, "Systeme 6.0.8", 11, TF_NORMAL);
    tx_draw(f, x + 88, y + 46, "1 Mo de memoire", 11, TF_NORMAL);
    tx_draw(f, x + 88, y + 60, "ROM 128 Ko", 11, TF_NORMAL);

    fb_hline(f, x + 10, y + 76, cw - 20, 1);
    tx_draw(f, x + 14, y + 82, "oldmac " VERSION_STR, 11, TF_BOLD);
    tx_draw(f, x + 14, y + 96, "simulateur de Macintosh compact", 10, TF_NORMAL);
    tx_draw(f, x + 14, y + 108, "ecrit en C, GTK4 et cairo", 10, TF_NORMAL);
    tx_draw(f, x + 14, y + 120, "@paristzou  -  juste pour les filles", 10, TF_NORMAL);

    fb_frame(f, x + cw / 2 - 34, y + w->h - TITLE_H - 34, 68, 18);
    tx_draw(f, x + cw / 2 - 14, y + w->h - TITLE_H - 30, "OK", 11, TF_BOLD);
}

/* --- alerte ------------------------------------------------------- */
static void draw_alert(fb_t *f, win_t *w)
{
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    fb_rect(f, x, y, w->w - 2, w->h - TITLE_H - 2, 0);
    /* triangle d'attention */
    fb_line(f, x + 20, y + 34, x + 34, y + 8, 1);
    fb_line(f, x + 34, y + 8, x + 48, y + 34, 1);
    fb_line(f, x + 20, y + 34, x + 48, y + 34, 1);
    fb_line(f, x + 24, y + 32, x + 34, y + 11, 1);
    fb_line(f, x + 44, y + 32, x + 34, y + 11, 1);
    fb_rect(f, x + 33, y + 16, 2, 9, 1);
    fb_rect(f, x + 33, y + 28, 2, 2, 1);
    for (int i = 0; i < alert_n && i < 4; i++)
        tx_draw(f, x + 58, y + 10 + i * 14, alert_lines[i], 11, TF_BOLD);
    fb_frame(f, x + w->w / 2 - 46, y + w->h - TITLE_H - 32, 60, 18);
    tx_draw(f, x + w->w / 2 - 22, y + w->h - TITLE_H - 28, "OK", 11, TF_BOLD);
    tx_draw(f, x + w->w / 2 + 20, y + w->h - TITLE_H - 28, "Annuler", 10, TF_NORMAL);
}

/* ------------------------------------------------------------------ */
/* --- BATMino : la carte vue depuis le Mac -------------------------- */
#define TEL_BTN_W      56
#define TEL_BTN_H      17
#define TEL_BTN_STRIDE 60
#define TEL_BTN_X0     5

static void tel_bouton_dessine(fb_t *f, int x, int y, int w, const char *label)
{
    fb_rect(f, x, y, w, TEL_BTN_H, 0);
    fb_frame(f, x, y, w, TEL_BTN_H);
    int taille = 10;
    int tw = tx_width(label, taille, TF_BOLD);
    if (tw > w - 6) { taille = 9; tw = tx_width(label, taille, TF_BOLD); }
    tx_draw(f, x + (w - tw) / 2, y + 3, label, taille, TF_BOLD);
}

static void draw_tel(fb_t *f, win_t *w, sys_t *s)
{
    (void)s;
    tel_t *t = &G_TEL;
    int x = w->x + 1, y = w->y + TITLE_H + 1;
    int cw = w->w - 2, ch = w->h - TITLE_H - 2;
    fb_rect(f, x, y, cw, ch, 0);

    /* la dalle de la carte, pixel pour pixel, sans rien inventer */
    int bx = x + (cw - TEL_W - 2) / 2, by = y + 5;
    /* un cadre noir de deux pixels : la dalle se detache sur la photo */
    fb_frame(f, bx - 2, by - 2, TEL_W + 5, TEL_H + 5);
    fb_frame(f, bx - 1, by - 1, TEL_W + 3, TEL_H + 3);
    fb_frame(f, bx, by, TEL_W + 2, TEL_H + 2);
    if (t->ecran_lu) {
        for (int py = 0; py < TEL_H; py++) {
            const uint8_t *ligne = t->ecran + py * (TEL_W / 8);
            for (int px = 0; px < TEL_W; px++)
                if ((ligne[px >> 3] >> (7 - (px & 7))) & 1)
                    fb_pset(f, bx + 1 + px, by + 1 + py, 1);
        }
    } else {
        tx_draw(f, bx + 12, by + TEL_H / 2 - 6, "pas encore d'image", 10, TF_NORMAL);
    }

    /* sous la dalle : le cable, puis ce que la carte raconte */
    char l[128];
    int ty = by + TEL_H + 8;
    snprintf(l, sizeof(l), "%.20s - %.34s", t->port, t->status);
    tx_draw(f, x + 8, ty, l, 10, TF_BOLD);
    if (t->etat_lu) {
        snprintf(l, sizeof(l), "page %d - %d elements - %.10s", t->page, t->items, t->profil);
        tx_draw(f, x + 8, ty + 13, l, 10, TF_NORMAL);
        snprintf(l, sizeof(l), "wifi %s - heure %s - v%.10s",
                 t->wifi == 1 ? "oui" : t->wifi == 0 ? "non" : "n.d.",
                 t->heure == 1 ? "oui" : t->heure == 0 ? "non" : "n.d.",
                 t->version);
        tx_draw(f, x + 8, ty + 26, l, 10, TF_NORMAL);
    } else {
        tx_draw(f, x + 8, ty + 13, "etat : n.d.", 10, TF_NORMAL);
        tx_draw(f, x + 8, ty + 26, "dalle : n.d.", 10, TF_NORMAL);
    }

    /* les quatre touches, comme sur la carte */
    int bby = y + ch - 24;
    tel_bouton_dessine(f, x + TEL_BTN_X0, bby, TEL_BTN_W, "< page");
    tel_bouton_dessine(f, x + TEL_BTN_X0 + TEL_BTN_STRIDE, bby, TEL_BTN_W, "page >");
    tel_bouton_dessine(f, x + TEL_BTN_X0 + 2 * TEL_BTN_STRIDE, bby, TEL_BTN_W, "bouton 1");
    tel_bouton_dessine(f, x + TEL_BTN_X0 + 3 * TEL_BTN_STRIDE, bby, TEL_BTN_W, "bouton 2");
}

void app_draw(win_t *w, fb_t *f)
{
    barre_titre(f, w);
    switch (w->kind) {
    case APP_FINDER:  draw_finder(f, w, &G); break;
    case APP_WRITE:   draw_write(f, w, &G); break;
    case APP_PAINT:   draw_paint(f, w); break;
    case APP_CONTROL: draw_control(f, w, &G); break;
    case APP_ABOUT:   draw_about(f, w); break;
    case APP_ALERT:   draw_alert(f, w); break;
    case APP_TEL:     draw_tel(f, w, &G); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* menus deroulants                                                   */
/* ------------------------------------------------------------------ */
static void draw_menu(fb_t *f, sys_t *s)
{
    if (s->menu.open < 0) return;
    int m = s->menu.open;
    int n = menu_nitems(m);
    int wmax = 60;
    for (int i = 0; i < n; i++) {
        const char *t = menu_item(m, i);
        if (!t) continue;
        int w = tx_width(t, 12, TF_NORMAL) + 34;
        if (w > wmax) wmax = w;
    }
    int ih = 16;
    int h = n * ih + 5;
    int x = menu_x[m];
    int y = MB_H - 1;
    if (x + wmax > SCR_W - 2) x = SCR_W - wmax - 2;

    /* ombre */
    fb_rect(f, x + 2, y + 2, wmax, h, 1);
    fb_rect(f, x, y, wmax, h, 0);
    fb_frame(f, x, y, wmax, h);

    for (int i = 0; i < n; i++) {
        const char *t = menu_item(m, i);
        if (!t) continue;
        int iy = y + 2 + i * ih;
        if (strcmp(t, "--") == 0) {
            fb_hline(f, x + 8, iy + ih / 2, wmax - 16, 1);
            continue;
        }
        int dis = menu_item_disabled(m, i);
        int sel = (s->menu.item == i);
        if (sel) fb_rect(f, x + 1, iy, wmax - 2, ih, 1);
        int tw = tx_width(t, 12, TF_NORMAL);
        if (sel) tx_draw_inv(f, x + 20, iy + 1, t, 12, TF_NORMAL);
        else {
            if (dis) fb_lightgray(f, x + 20, iy + 1, tw, 12);
            tx_draw(f, x + 20, iy + 1, t, 12, dis ? TF_NORMAL : TF_NORMAL);
            if (dis) {
                for (int k = 0; k < tw; k++) fb_pset(f, x + 20 + k, iy + 6, 0);
                tx_draw(f, x + 20, iy + 1, t, 12, TF_NORMAL);
            }
        }
        if (menu_item_checked(m, i)) {
            fb_line(f, x + 8, iy + 8, x + 11, iy + 11, 1);
            fb_line(f, x + 11, iy + 11, x + 16, iy + 4, 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/* bureau, barre, curseur, demarrage                                  */
/* ------------------------------------------------------------------ */
static void draw_desktop(fb_t *f, sys_t *s)
{
    (void)s;
    fb_gray(f, 0, 0, SCR_W, SCR_H);
    for (int i = 0; i < icon_n; i++) {
        icon_t *ic = &icons[i];
        int selected = (icon_sel == i);
        if (selected) {
            fb_rect(f, ic->x - 4, ic->y - 4, 38, 40, 0);
            fb_frame(f, ic->x - 4, ic->y - 4, 38, 40);
        }
        icone_app(f, ic->kind, ic->x, ic->y);
        int tw = tx_width(ic->name, 10, TF_NORMAL);
        fb_rect(f, ic->x + 12 - tw / 2 - 2, ic->y + 36, tw + 4, 13, 0);
        tx_draw(f, ic->x + 12 - tw / 2, ic->y + 37, ic->name, 10, TF_NORMAL);
        if (selected) {
            fb_frame(f, ic->x - 4, ic->y - 4, 38, 56);
        }
    }
}

static void happy_mac(fb_t *f, int x, int y)
{
    /* visage souriant : la disquette qui dit bonjour */
    fb_rrect(f, x, y, 44, 44, 4, 1);
    fb_rrect(f, x + 3, y + 3, 38, 38, 3, 0);
    fb_rect(f, x + 11, y + 14, 3, 7, 1);
    fb_rect(f, x + 30, y + 14, 3, 7, 1);
    for (int i = 0; i < 12; i++) {
        int yy = y + 28 + (i < 6 ? i : 11 - i);
        fb_pset(f, x + 16 + i, yy, 1);
    }
    fb_rect(f, x + 3, y + 33, 38, 8, 1);
    fb_rect(f, x + 5, y + 35, 34, 4, 0);
}

void app_boot_draw(sys_t *s, fb_t *f, int64_t now_ms)
{
    fb_clear(f);
    /* machine eteinte : l'ecran est noir, il ne reste que la diode */
    if (!s->ecran_allume) {
        fb_fill(f);
        fb_circle(f, SCR_W / 2, SCR_H / 2 - 10, 9, 0);
        fb_circle(f, SCR_W / 2, SCR_H / 2 - 10, 8, 1);
        fb_vline(f, SCR_W / 2, SCR_H / 2 - 16, 6, 0);
        const char *msg = "Cliquez sur la machine pour la rallumer";
        tx_draw(f, SCR_W / 2 - tx_width(msg, 11, TF_BOLD) / 2, SCR_H - 44, msg, 11, TF_BOLD);
        return;
    }
    int64_t dt = now_ms - s->boot_ms;
    if (dt < 1100 && !s->rapide) {
        /* disquette inconnue : le Mac cherche un systeme */
        int blink = ((dt / 220) % 2) == 0;
        fb_rrect(f, SCR_W / 2 - 24, SCR_H / 2 - 30, 48, 52, 4, 1);
        fb_rrect(f, SCR_W / 2 - 21, SCR_H / 2 - 27, 42, 46, 3, 0);
        fb_rect(f, SCR_W / 2 - 12, SCR_H / 2 - 20, 24, 14, 1);
        fb_rect(f, SCR_W / 2 - 10, SCR_H / 2 - 18, 20, 10, 0);
        if (blink)
            tx_draw(f, SCR_W / 2 - 4, SCR_H / 2 - 2, "?", 22, TF_BOLD);
        const char *msg = "Inserez le disque systeme";
        tx_draw(f, SCR_W / 2 - tx_width(msg, 11, TF_BOLD) / 2, SCR_H - 40, msg, 11, TF_BOLD);
        return;
    }
    if (dt < 3200 && !s->rapide) {
        happy_mac(f, SCR_W / 2 - 22, SCR_H / 2 - 50);
        const char *msg = "Bienvenue sur Macintosh";
        tx_draw(f, SCR_W / 2 - tx_width(msg, 12, TF_BOLD) / 2, SCR_H / 2 + 6, msg, 12, TF_BOLD);
        /* barre de progression du systeme */
        int bw = 180;
        int bx = SCR_W / 2 - bw / 2, by = SCR_H / 2 + 30;
        fb_frame(f, bx, by, bw, 10);
        int p = (int)((dt - 1100) * bw / 2100);
        if (p < 0) p = 0;
        if (p > bw - 2) p = bw - 2;
        fb_rect(f, bx + 1, by + 1, p, 8, 1);
        tx_draw(f, bx + bw + 6, by + 1, "Systeme 6.0.8", 10, TF_NORMAL);
        return;
    }
    /* bureau */
    draw_desktop(f, s);
    barre_menus(f, s);
    for (int i = 0; i < s->wm.n; i++) {
        win_t *w = wm_by_id(&s->wm, s->wm.z[i]);
        if (w && w->visible) app_draw(w, f);
    }
    draw_menu(f, s);
    /* message de service, discret, en bas au centre */
    if (s->statut[0]) {
        int tw = tx_width(s->statut, 10, TF_NORMAL);
        fb_rect(f, SCR_W / 2 - tw / 2 - 4, SCR_H - 18, tw + 8, 14, 0);
        fb_frame(f, SCR_W / 2 - tw / 2 - 4, SCR_H - 18, tw + 8, 14);
        tx_draw(f, SCR_W / 2 - tw / 2, SCR_H - 16, s->statut, 10, TF_NORMAL);
    }
    if (s->curseur_lu) curseur(f, s->curseur_x, s->curseur_y);
}

void app_render(sys_t *s, fb_t *f)
{
    app_boot_draw(s, f, now_ms_cache);
}

/* ------------------------------------------------------------------ */
/* ouverture d'applications                                           */
/* ------------------------------------------------------------------ */
static void init_write(write_st *st, const char *name, const char *txt)
{
    memset(st, 0, sizeof(*st));
    snprintf(st->name, sizeof(st->name), "%s", name);
    if (txt) {
        snprintf(st->text, sizeof(st->text), "%s", txt);
        st->len = (int)strlen(st->text);
        st->caret = st->len;
    }
}

static void init_paint(paint_st *st, const char *name)
{
    memset(st, 0, sizeof(*st));
    st->w = 320;
    st->h = 200;
    snprintf(st->name, sizeof(st->name), "%s", name);
    /* une feuille blanche */
}

/* le dessin de la photo : un chien qui dit au revoir, et son mot */
void scene_paint_demo(win_t *w)
{
    if (!w || w->kind != APP_PAINT || !w->st) return;
    paint_st *st = (paint_st *)w->st;
    fb_t t = { st->bits, st->w, st->h, st->w };
    fb_clear(&t);

    /* la tete */
    for (int i = 0; i < 2; i++) fb_circle(&t, 96, 96, 34 - i, 1);
    fb_circle(&t, 96, 96, 31, 0);
    /* les oreilles */
    fb_circle(&t, 72, 68, 13, 1);
    fb_circle(&t, 72, 68, 10, 0);
    fb_circle(&t, 120, 68, 13, 1);
    fb_circle(&t, 120, 68, 10, 0);
    /* les yeux */
    fb_circle(&t, 85, 90, 3, 1);
    fb_circle(&t, 107, 90, 3, 1);
    /* le museau */
    fb_circle(&t, 96, 108, 6, 1);
    fb_vline(&t, 96, 114, 8, 1);
    fb_line(&t, 96, 122, 88, 127, 1);
    fb_line(&t, 96, 122, 104, 127, 1);
    fb_rect(&t, 94, 124, 5, 4, 1);
    /* moustaches */
    fb_line(&t, 74, 104, 86, 106, 1);
    fb_line(&t, 74, 112, 86, 111, 1);
    fb_line(&t, 118, 104, 106, 106, 1);
    fb_line(&t, 118, 112, 106, 111, 1);
    /* le mot de la photo, ecrit a la main dans le dessin */
    tx_draw(&t, 168, 26, "good bye.......", 13, TF_BOLD);
    tx_draw(&t, 210, 46, "dad!", 15, TF_BOLD);
    /* un os, pour les jours heureux */
    fb_rect(&t, 190, 150, 60, 6, 0);
    fb_frame(&t, 190, 150, 60, 6);
    fb_circle(&t, 190, 147, 5, 1); fb_circle(&t, 190, 158, 5, 1);
    fb_circle(&t, 250, 147, 5, 1); fb_circle(&t, 250, 158, 5, 1);
    fb_circle(&t, 190, 152, 4, 0); fb_circle(&t, 250, 152, 4, 0);
    st->dirty = 0;
    snprintf(st->name, sizeof(st->name), "good bye dad");
    snprintf(w->title, sizeof(w->title), "good bye dad");
}

void app_open(sys_t *s, int kind)
{
    int x = 34 + (s->wm.next_y % 4) * 32;
    int y = MB_H + 10 + (s->wm.next_y % 4) * 22;
    s->wm.next_y++;
    win_t *w = NULL;
    switch (kind) {
    case APP_FINDER:
        w = wm_open(&s->wm, APP_FINDER, "Macintosh HD", 44, 40, 300, 200);
        w->st = calloc(1, sizeof(finder_st));
        break;
    case APP_WRITE: {
        w = wm_open(&s->wm, APP_WRITE, "Sans titre", x, y, 330, 190);
        write_st *st = calloc(1, sizeof(write_st));
        init_write(st, "Sans titre", "");
        w->st = st;
        break;
    }
    case APP_PAINT: {
        w = wm_open(&s->wm, APP_PAINT, "Sans titre", x, y, 380, 250);
        paint_st *st = calloc(1, sizeof(paint_st));
        init_paint(st, "Sans titre");
        w->st = st;
        break;
    }
    case APP_CONTROL:
        w = wm_open(&s->wm, APP_CONTROL, "Tableau de bord", 90, 44, 320, 280);
        break;
    case APP_ABOUT:
        w = wm_open(&s->wm, APP_ABOUT, "A propos", 106, 60, 300, 210);
        break;
    case APP_ALERT:
        w = wm_open(&s->wm, APP_ALERT, "Alerte", 130, 120, 260, 110);
        break;
    case APP_TEL: {
        w = wm_open(&s->wm, APP_TEL, "BATMino", 150, 24, 248, 306);
        /* on branche le cable et on demande ce qu'elle a sur la dalle */
        tel_ouvrir(&G_TEL);
        tel_demander_etat(&G_TEL);
        tel_demander_ecran(&G_TEL);
        break;
    }
    default:
        break;
    }
    /* aucune fenetre ne doit sortir de l'ecran */
    if (w) {
        if (w->x + w->w > SCR_W - 4) w->x = SCR_W - 4 - w->w;
        if (w->y + w->h > SCR_H - 4) w->y = SCR_H - 4 - w->h;
        if (w->x < 2) w->x = 2;
        if (w->y < MB_H + 2) w->y = MB_H + 2;
    }
}

static void alerte(sys_t *s, const char *a, const char *b, const char *c, const char *d)
{
    alert_lines[0] = a; alert_lines[1] = b; alert_lines[2] = c; alert_lines[3] = d;
    alert_n = 0;
    const char *ls[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) if (ls[i]) alert_n = i + 1;
    snprintf(alerte_txt, sizeof(alerte_txt), "%s", a ? a : "");
    /* une seule alerte a la fois */
    for (int i = 0; i < s->wm.n; i++)
        if (s->wm.w[i].kind == APP_ALERT) { wm_raise(&s->wm, s->wm.w[i].id); return; }
    wm_open(&s->wm, APP_ALERT, "Alerte", 130, 120, 260, 110);
}

void app_close(win_t *w)
{
    /* la telecommande garde son cable : on ferme la fenetre, pas la carte */
    if (!w->st) return;
    if (w->kind == APP_WRITE || w->kind == APP_PAINT) free(w->st);
    else if (w->kind == APP_FINDER) free(w->st);
    w->st = NULL;
}

/* ------------------------------------------------------------------ */
/* clics                                                              */
/* ------------------------------------------------------------------ */
static void write_insert(write_st *st, const char *txt)
{
    int n = (int)strlen(txt);
    if (st->len + n + 1 >= MAX_TXT) return;
    memmove(st->text + st->caret + n, st->text + st->caret, (size_t)(st->len - st->caret + 1));
    memcpy(st->text + st->caret, txt, (size_t)n);
    st->caret += n;
    st->len += n;
    st->dirty = 1;
}

static void paint_stroke(sys_t *s, win_t *w, int lx, int ly)
{
    (void)s;
    paint_st *st = (paint_st *)w->st;
    int px = lx - (PAINT_PAL + 4), py = ly - (TITLE_H + 4);
    if (px < 0 || py < 0 || px >= st->w || py >= st->h) return;
    if (st->tool == TOOL_CRAYON || st->tool == TOOL_GOMME) {
        int v = (st->tool == TOOL_GOMME) ? 0 : 1;
        if (st->down) {
            int x0 = st->lx, y0 = st->ly;
            int dx = abs(px - x0), sx = x0 < px ? 1 : -1;
            int dy = -abs(py - y0), sy = y0 < py ? 1 : -1;
            int err = dx + dy, x = x0, y = y0;
            for (;;) {
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++)
                        if (x + i >= 0 && y + j >= 0 && x + i < st->w && y + j < st->h)
                            st->bits[(size_t)(y + j) * st->w + (x + i)] = (uint8_t)v;
                if (x == px && y == py) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x += sx; }
                if (e2 <= dx) { err += dx; y += sy; }
            }
        }
        st->lx = px; st->ly = py;
        st->dirty = 1;
    }
}

void app_click(sys_t *s, win_t *w, int lx, int ly, int double_click)
{
    if (!w) return;
    /* clic dans la case de fermeture */
    if (lx >= 4 && lx <= 15 && ly >= 3 && ly <= 15) {
        if (w->kind == APP_ALERT) { }
        wm_close(&s->wm, w->id);
        return;
    }
    if (ly < TITLE_H) {
        wm_raise(&s->wm, w->id);
        return;
    }
    int cy = ly - TITLE_H - 1;

    switch (w->kind) {
    case APP_FINDER: {
        finder_st *st = (finder_st *)w->st;
        int row = 16;
        int k = (cy - 14) / row;
        if (cy >= 14 && k >= 0 && k < s->disk.n + 1) {
            st->sel = k;
            if (double_click) {
                if (k == 0) app_open(s, APP_FINDER);
                else {
                    const char *n = s->disk.f[k - 1].name;
                    int kind = s->disk.f[k - 1].type == FT_TEXT ? APP_WRITE : APP_PAINT;
                    app_open(s, kind);
                    win_t *nw = wm_top_visible(&s->wm);
                    if (nw && nw->st) {
                        if (kind == APP_WRITE) {
                            int len = 0;
                            char *t = disk_read(&s->disk, n, &len);
                            init_write((write_st *)nw->st, n, t ? t : "");
                            free(t);
                        } else {
                            int pw = 0, ph = 0;
                            uint8_t *b = disk_read_paint(&s->disk, n, &pw, &ph);
                            if (b) {
                                paint_st *ps = (paint_st *)nw->st;
                                int cw = pw < ps->w ? pw : ps->w;
                                int chh = ph < ps->h ? ph : ps->h;
                                for (int j = 0; j < chh; j++)
                                    memcpy(ps->bits + (size_t)j * ps->w, b + (size_t)j * pw, (size_t)cw);
                                free(b);
                                snprintf(ps->name, sizeof(ps->name), "%s", n);
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case APP_WRITE: {
        write_st *st = (write_st *)w->st;
        /* clic dans le texte : le curseur va a la ligne cliquee (approx.) */
        int lh = 14, marge = 6;
        int ligne = (cy - 8) / lh;
        int col = (lx - marge) / 7;
        if (col < 0) col = 0;
        int l = 0, i = 0, pos = 0;
        for (i = 0; i < st->len && l < ligne; i++) if (st->text[i] == '\n') l++;
        pos = i + col;
        if (pos > st->len) pos = st->len;
        if (pos < 0) pos = 0;
        st->caret = pos;
        break;
    }
    case APP_PAINT: {
        paint_st *st = (paint_st *)w->st;
        if (lx >= 5 && lx <= 30) {
            int idx = (cy - 5) / 26;
            if (idx >= 0 && idx < 5) st->tool = idx;
            break;
        }
        st->down = 1;
        paint_stroke(s, w, lx, ly);
        break;
    }
    case APP_CONTROL: {
        if (cy >= 26 && cy <= 34) {
            int v = (lx - 13) * 100 / (w->w - 38);
            s->bright = v < 0 ? 0 : (v > 100 ? 100 : v);
        } else if (cy >= 70 && cy <= 78) {
            int v = (lx - 13) * 100 / (w->w - 38);
            s->son = v < 0 ? 0 : (v > 100 ? 100 : v);
        }
        break;
    }
    case APP_TEL: {
        int bby = (w->h - TITLE_H - 2) - 24;
        if (cy >= bby && cy < bby + TEL_BTN_H && lx >= TEL_BTN_X0) {
            int k = (lx - TEL_BTN_X0) / TEL_BTN_STRIDE;
            if (k == 0)      tel_page(&G_TEL, tel_page_suivante(&G_TEL, -1));
            else if (k == 1) tel_page(&G_TEL, tel_page_suivante(&G_TEL, 1));
            else if (k == 2) tel_bouton(&G_TEL, 1, TEL_BOUTON_COURT);
            else if (k == 3) tel_bouton(&G_TEL, 2, TEL_BOUTON_COURT);
            break;
        }
        /* clic sur la dalle : redemander l'image, comme un rafraichissement */
        int bx = (w->w - 2 - TEL_W - 2) / 2;
        if (lx >= bx && lx < bx + TEL_W + 2 && cy >= 5 && cy < 5 + TEL_H + 2)
            tel_demander_ecran(&G_TEL);
        break;
    }
    case APP_ABOUT:
    case APP_ALERT:
        wm_close(&s->wm, w->id);
        break;
    default: break;
    }
}

static void icone_click(sys_t *s, int x, int y, int dbl)
{
    for (int i = 0; i < icon_n; i++) {
        icon_t *ic = &icons[i];
        if (x >= ic->x - 20 && x <= ic->x + 50 && y >= ic->y - 6 && y <= ic->y + 44) {
            icon_sel = i;
            if (dbl) {
                if (ic->kind == APP_WRITE) app_open(s, APP_WRITE);
                else if (ic->kind == APP_PAINT) app_open(s, APP_PAINT);
                else if (ic->kind == APP_TEL) app_open(s, APP_TEL);
                else if (ic->kind == -1) app_open(s, APP_FINDER);
                else if (ic->kind == -2) alerte(s, "La corbeille est vide.", "Vider la corbeille depuis", "le menu Special.", NULL);
            }
            return;
        }
    }
    icon_sel = -1;
}

void app_click_screen(sys_t *s, int x, int y, int dbl)
{
    if (!s->ecran_allume) {          /* rallumer la machine */
        s->ecran_allume = 1;
        s->boot_phase = 0;
        s->rapide = 0;
        s->boot_ms = now_ms_cache;
        return;
    }
    if (s->boot_phase < 2) { s->rapide = 1; return; }

    /* barre de menus */
    if (y < MB_H) {
        for (int i = 0; i < 5; i++) {
            if (x >= menu_x[i] && x < menu_x[i] + menu_w[i]) {
                s->menu.open = i;
                s->menu.item = -1;
                return;
            }
        }
        s->menu.open = -1;
        return;
    }
    /* menu ouvert : on choisit */
    if (s->menu.open >= 0 && s->menu.item >= 0) {
        int m = s->menu.open, it = s->menu.item;
        s->menu.open = -1; s->menu.item = -1;
        app_menu(s, m, it);
        return;
    }
    s->menu.open = -1;

    win_t *w = wm_at(&s->wm, x, y);
    if (w) {
        wm_raise(&s->wm, w->id);
        app_click(s, w, x - w->x, y - w->y, dbl);
        return;
    }
    icone_click(s, x, y, dbl);
}

/* ------------------------------------------------------------------ */
/* clavier                                                            */
/* ------------------------------------------------------------------ */
void app_key_screen(sys_t *s, unsigned key, const char *txt)
{
    if (s->boot_phase < 2) { s->rapide = 1; return; }
    win_t *w = wm_top_visible(&s->wm);
    if (!w) return;
    switch (w->kind) {
    case APP_WRITE: {
        write_st *st = (write_st *)w->st;
        if (key == 0xff08) {           /* retour arriere */
            if (st->caret > 0) {
                memmove(st->text + st->caret - 1, st->text + st->caret,
                        (size_t)(st->len - st->caret + 1));
                st->caret--; st->len--; st->dirty = 1;
            }
        } else if (key == 0xff1b) {     /* echap */
            wm_close(&s->wm, w->id);
        } else if (txt && txt[0] >= 32) {
            write_insert(st, txt);
        }
        break;
    }
    case APP_ALERT:
        if (key == 0xff0d || key == 0xff1b) wm_close(&s->wm, w->id);
        break;
    case APP_PAINT: {
        paint_st *st = (paint_st *)w->st;
        if (key >= '1' && key <= '5') st->tool = (int)(key - '1');
        break;
    }
    case APP_TEL:
        if (key == 0xff51)      tel_page(&G_TEL, tel_page_suivante(&G_TEL, -1));
        else if (key == 0xff53) tel_page(&G_TEL, tel_page_suivante(&G_TEL, 1));
        else if (key == 'b')    tel_bouton(&G_TEL, 1, TEL_BOUTON_COURT);
        else if (key == 'n')    tel_bouton(&G_TEL, 2, TEL_BOUTON_COURT);
        else if (key == 'r')    tel_demander_ecran(&G_TEL);
        else if (key == 'e')    tel_demander_etat(&G_TEL);
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* actions de menu                                                    */
/* ------------------------------------------------------------------ */
static void sauver_courant(sys_t *s)
{
    win_t *w = wm_top_visible(&s->wm);
    if (!w) return;
    if (w->kind == APP_WRITE) {
        write_st *st = (write_st *)w->st;
        if (strcmp(st->name, "Sans titre") == 0 || st->name[0] == 0) {
            time_t t = time(NULL);
            struct tm tm;
            localtime_r(&t, &tm);
            snprintf(st->name, sizeof(st->name), "Sans titre %02d%02d", tm.tm_hour, tm.tm_min);
            snprintf(w->title, sizeof(w->title), "%.27s", st->name);
        }
        disk_write_text(&s->disk, st->name, st->text);
        st->dirty = 0;
        snprintf(s->statut, sizeof(s->statut), "Enregistre : %s", st->name);
        s->statut_ms = 2500;
    } else if (w->kind == APP_PAINT) {
        paint_st *st = (paint_st *)w->st;
        if (strcmp(st->name, "Sans titre") == 0) {
            time_t t = time(NULL);
            struct tm tm;
            localtime_r(&t, &tm);
            snprintf(st->name, sizeof(st->name), "Dessin %02d%02d", tm.tm_hour, tm.tm_min);
            snprintf(w->title, sizeof(w->title), "%.27s", st->name);
        }
        disk_write_paint(&s->disk, st->name, st->bits, st->w, st->h);
        st->dirty = 0;
        snprintf(s->statut, sizeof(s->statut), "Enregistre : %s", st->name);
        s->statut_ms = 2500;
    }
}

void app_menu(sys_t *s, int m, int item)
{
    const char *t = menu_item(m, item);
    if (!t) return;

    if (m == 0) {
        if (strcmp(t, "A propos de ce Mac") == 0) app_open(s, APP_ABOUT);
        else if (strcmp(t, "Tableau de bord") == 0) app_open(s, APP_CONTROL);
        else if (strcmp(t, "Horloge") == 0) s->horloge = !s->horloge;
        else if (strcmp(t, "MacWrite") == 0) app_open(s, APP_WRITE);
        else if (strcmp(t, "MacPaint") == 0) app_open(s, APP_PAINT);
        else if (strcmp(t, "Le Finder") == 0) app_open(s, APP_FINDER);
        else if (strcmp(t, "Redemarrer") == 0) {
            s->wm.n = 0; s->boot_phase = 0; s->boot_ms = now_ms_cache; s->rapide = 0;
        } else if (strcmp(t, "Eteindre") == 0) {
            s->ecran_allume = 0; s->wm.n = 0;
        }
        return;
    }
    if (m == 1) {
        if (strcmp(t, "Nouveau") == 0) {
            win_t *w = wm_top_visible(&s->wm);
            if (w && w->kind == APP_PAINT) app_open(s, APP_PAINT);
            else app_open(s, APP_WRITE);
        } else if (strcmp(t, "Ouvrir...") == 0) app_open(s, APP_FINDER);
        else if (strcmp(t, "Fermer") == 0) {
            win_t *w = wm_top_visible(&s->wm);
            if (w) wm_close(&s->wm, w->id);
        } else if (strcmp(t, "Enregistrer") == 0) sauver_courant(s);
        else if (strcmp(t, "Mettre a la corbeille") == 0) {
            win_t *w = wm_top_visible(&s->wm);
            if (w && (w->kind == APP_WRITE || w->kind == APP_PAINT)) {
                char nom[40];
                if (w->kind == APP_WRITE) snprintf(nom, sizeof(nom), "%s", ((write_st *)w->st)->name);
                else snprintf(nom, sizeof(nom), "%s", ((paint_st *)w->st)->name);
                disk_trash(&s->disk, nom);
                wm_close(&s->wm, w->id);
                snprintf(s->statut, sizeof(s->statut), "Corbeille : %.44s", nom);
                s->statut_ms = 2500;
            }
        } else if (strcmp(t, "Imprimer l'ecran") == 0) {
            snprintf(s->statut, sizeof(s->statut), "Ecran imprime sur le disque");
            s->statut_ms = 2500;
        }
        return;
    }
    if (m == 2) {
        win_t *w = wm_top_visible(&s->wm);
        if (!w) return;
        if (w->kind == APP_WRITE) {
            write_st *st = (write_st *)w->st;
            if (strcmp(t, "Tout effacer") == 0) {
                st->len = 0; st->caret = 0; st->text[0] = 0; st->dirty = 1;
            } else if (strcmp(t, "Copier") == 0) {
                if (st->len > 0) {
                    int n = st->len < (int)sizeof(clip) - 1 ? st->len : (int)sizeof(clip) - 1;
                    memcpy(clip, st->text, (size_t)n); clip[n] = 0; clip_n = n;
                }
            } else if (strcmp(t, "Couper") == 0) {
                int n = st->len < (int)sizeof(clip) - 1 ? st->len : (int)sizeof(clip) - 1;
                memcpy(clip, st->text, (size_t)n); clip[n] = 0; clip_n = n;
                st->len = 0; st->caret = 0; st->text[0] = 0; st->dirty = 1;
            } else if (strcmp(t, "Coller") == 0 && clip_n > 0) {
                write_insert(st, clip);
            }
        }
        return;
    }
    if (m == 4) {
        if (strcmp(t, "BATMino") == 0) {
            app_open(s, APP_TEL);
        } else if (strcmp(t, "Vider la corbeille") == 0) {
            disk_empty_trash(&s->disk);
            snprintf(s->statut, sizeof(s->statut), "Corbeille videe");
            s->statut_ms = 2500;
        } else if (strcmp(t, "A propos du projet") == 0) {
            app_open(s, APP_ABOUT);
        }
        return;
    }
    if (m == 3) {
        if (strcmp(t, "Rafraichir") == 0) disk_rescan(&s->disk);
        else if (strcmp(t, "Infos du disque") == 0) {
            char a[64], b[64], c[64];
            snprintf(a, sizeof(a), "Macintosh HD : %d Ko", disk_capacity_ko());
            snprintf(b, sizeof(b), "%d Ko utilises, %d Ko libres", disk_used_ko(), disk_free_ko());
            snprintf(c, sizeof(c), "%d document(s).", s->disk.n);
            alerte(s, a, b, c, NULL);
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
void sys_tick(sys_t *s, int64_t now_ms)
{
    now_ms_cache = now_ms;
    int64_t dt = now_ms - s->boot_ms;
    if (s->boot_phase < 2) {
        if (s->rapide) s->boot_phase = 2;
        else if (dt > 3300) s->boot_phase = 2;
        else if (dt > 1100) s->boot_phase = 1;
    }
    caret_on = ((now_ms / 500) % 2) == 0;
    if (s->statut_ms > 0) {
        s->statut_ms -= 16;
        if (s->statut_ms <= 0) s->statut[0] = 0;
    }
}

/* ------------------------------------------------------------------ */
/* survol, glisser, relacher                                          */
/* ------------------------------------------------------------------ */
void app_hover(sys_t *s, int x, int y)
{
    if (s->menu.open < 0) return;
    int m = s->menu.open;
    /* survoler un autre titre de menu change de menu */
    if (y < MB_H) {
        for (int i = 0; i < 5; i++)
            if (x >= menu_x[i] && x < menu_x[i] + menu_w[i]) {
                s->menu.open = i;
                s->menu.item = -1;
                return;
            }
        return;
    }
    int ih = 16;
    int y0 = MB_H + 1;
    int n = menu_nitems(m);
    for (int i = 0; i < n; i++) {
        int iy = y0 + i * ih;
        if (y >= iy && y < iy + ih) {
            const char *t = menu_item(m, i);
            s->menu.item = (t && strcmp(t, "--") != 0) ? i : -1;
            return;
        }
    }
    s->menu.item = -1;
}

int app_debug_len(win_t *w)
{
    if (!w) return -1;
    if (w->kind == APP_WRITE) return ((write_st *)w->st)->len;
    return -2;
}

void app_release(win_t *w)
{
    if (w->kind == APP_PAINT && w->st) ((paint_st *)w->st)->down = 0;
}

void app_drag(win_t *w, int lx, int ly)
{
    if (w->kind == APP_PAINT && w->st) paint_stroke(&G, w, lx, ly);
}

/* remplit la fenetre MacWrite de la scene de demonstration */
void scene_write_fill(win_t *w)
{
    if (!w || w->kind != APP_WRITE || !w->st) return;
    write_st *st = (write_st *)w->st;
    const char *txt =
        "good bye....... dad!\n\n"
        "Ce Macintosh a 39 ans et il demarre encore.\n"
        "Tape n'importe quoi : le curseur clignote,\n"
        "le texte se range, et rien ne se perd tant\n"
        "que tu n'as pas enregistre.\n\n"
        "Fichier > Enregistrer : le document part\n"
        "sur le disque de la machine, en vrai.\n";
    init_write(st, "good bye dad", txt);
    snprintf(w->title, sizeof(w->title), "good bye dad");
}
