/*  oldmac.h — simulateur Macintosh compact (512 x 342, 1 bit)
 *
 *  Le projet est autonome : aucun code partage avec les firmwares ESP32.
 *  Tout ce qui s'affiche sort d'un framebuffer 1 bit, comme la dalle 9 pouces.
 */
#ifndef OLDMAC_H
#define OLDMAC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct _cairo cairo_t;   /* evite de trainer cairo.h partout */

#define SCR_W 512
#define SCR_H 342
#define MB_H 20                 /* hauteur de la barre de menus        */
#define TITLE_H 19              /* hauteur d'une barre de titre        */
#define MB_POPUP_Y (MB_H + 1)   /* haut d'un menu deroulant            */
#define VERSION_STR "1.1.0"

/* ------------------------------------------------------------------ */
/* framebuffer 1 bit : 1 octet par pixel, 0 = blanc, 1 = noir          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t *p;
    int w, h, stride;
} fb_t;

void fb_init(fb_t *f, int w, int h);
void fb_free(fb_t *f);
void fb_clear(fb_t *f);                    /* tout blanc */
void fb_fill(fb_t *f);                     /* tout noir */
void fb_pset(fb_t *f, int x, int y, int v);
int  fb_pget(fb_t *f, int x, int y);
void fb_rect(fb_t *f, int x, int y, int w, int h, int v);
void fb_frame(fb_t *f, int x, int y, int w, int h);
void fb_hline(fb_t *f, int x, int y, int len, int v);
void fb_vline(fb_t *f, int x, int y, int len, int v);
void fb_line(fb_t *f, int x0, int y0, int x1, int y1, int v);
void fb_circle(fb_t *f, int cx, int cy, int r, int v);
void fb_rrect(fb_t *f, int x, int y, int w, int h, int r, int v);
void fb_rframe(fb_t *f, int x, int y, int w, int h, int r);
void fb_gray(fb_t *f, int x, int y, int w, int h);      /* trame 50 % */
void fb_darkgray(fb_t *f, int x, int y, int w, int h);  /* trame 75 % */
void fb_lightgray(fb_t *f, int x, int y, int w, int h); /* trame 25 % */
void fb_invert(fb_t *f, int x, int y, int w, int h);
void fb_copy(fb_t *dst, fb_t *src, int dx, int dy);
int  fb_ink(fb_t *f, int x, int y, int w, int h);

/* texte : rendu par Pango en 1 bit, avec cache */
enum { TF_NORMAL = 0, TF_BOLD = 1 };
int  tx_width(const char *s, int size, int style);
int  tx_height(int size, int style);
void tx_draw(fb_t *f, int x, int y, const char *s, int size, int style);
void tx_draw_inv(fb_t *f, int x, int y, const char *s, int size, int style);
void tx_cache_clear(void);

/* ------------------------------------------------------------------ */
/* disque virtuel (~/.local/share/oldmac/disk)                        */
/* ------------------------------------------------------------------ */
#define DISK_MAX 24
#define NAME_MAX_LEN 40

typedef enum { FT_TEXT, FT_PAINT, FT_FOLDER } ftype_t;
typedef struct {
    char  name[NAME_MAX_LEN];   /* nom affiche, sans extension */
    ftype_t type;
    int   size;                 /* octets */
    char  path[512];
} dfile_t;

typedef struct {
    dfile_t f[DISK_MAX];
    int n;
    char root[400];
    int trash_n;
    char trash_names[DISK_MAX][NAME_MAX_LEN];
} disk_t;

void disk_init(disk_t *d);
void disk_rescan(disk_t *d);
int  disk_write_text(disk_t *d, const char *name, const char *data);
char *disk_read(disk_t *d, const char *name, int *len);
int  disk_write_paint(disk_t *d, const char *name, const uint8_t *bits, int w, int h);
uint8_t *disk_read_paint(disk_t *d, const char *name, int *w, int *h);
void disk_trash(disk_t *d, const char *name);
void disk_empty_trash(disk_t *d);
int  disk_free_ko(void);
int  disk_used_ko(void);
int  disk_capacity_ko(void);

/* ------------------------------------------------------------------ */
/* la telecommande : le Mac pilote la carte BATMino                   */
/* ------------------------------------------------------------------ */
#define TEL_W 200
#define TEL_H 200
#define TEL_FB_OCTETS (TEL_W * (TEL_H / 8))       /* 5000, comme la dalle */
#define TEL_PORT_DEFAUT "/dev/ttyACM0"
#define TEL_RX_MAX 12832                          /* une trame fait 10 003 c */

enum { TEL_BOUTON_COURT = 0, TEL_BOUTON_DOUBLE, TEL_BOUTON_LONG, TEL_BOUTON_R3 };
#define TEL_GESTES 4

enum { TEL_ATT_RIEN = 0, TEL_ATT_ETAT, TEL_ATT_PAGE, TEL_ATT_GESTE, TEL_ATT_ECRAN };

typedef struct {
    int  fd, ouvert;          /* le port, et s'il est ouvert             */
    int  joint;               /* la carte a repondu au moins une fois    */
    int  muet;                /* elle a cesse de repondre                */
    char port[64];
    char status[64];          /* ce qu'on affiche sous la dalle          */

    /* ce que la carte a dit */
    int  page, curseur, mode, items, hist, top, solde_ct;
    int  wifi, heure;         /* 1 oui, 0 non, -1 pas dit               */
    char profil[24], version[16];
    int  etat_lu;

    /* la dalle de la carte, 200 x 200 en 1 bit, bit de gauche en premier */
    uint8_t ecran[TEL_FB_OCTETS];
    int  ecran_lu;

    /* ce qu'on lui demande : une seule commande en vol a la fois */
    int  dem_p;               /* page a demander, -1 = rien              */
    int  dem_btn, dem_geste;  /* bouton et geste a jouer                 */
    int  dem_ecran;           /* trame a redemander                      */
    int  attente;             /* ce qui est en vol (TEL_ATT_*)           */
    int  envois, lignes;      /* compteurs, pour les tests               */
    int64_t attente_ms, dernier_etat_ms, dernier_essai_ms, dernier_rx_ms;

    char rx[TEL_RX_MAX];
    size_t rx_n;
} tel_t;

extern tel_t G_TEL;
void  tel_init(tel_t *t, const char *port);
int   tel_ouvrir(tel_t *t);
void  tel_fermer(tel_t *t);
void  tel_attacher(tel_t *t, int fd);     /* tests : un faux brut pret   */
int   tel_poll(tel_t *t, int64_t now_ms); /* 1 = quelque chose a redessiner */
void  tel_demander_etat(tel_t *t);
void  tel_demander_ecran(tel_t *t);
void  tel_page(tel_t *t, int n);
void  tel_bouton(tel_t *t, int n, int geste);
int   tel_page_suivante(const tel_t *t, int pas);
const char *tel_geste_nom(int geste);

/* ------------------------------------------------------------------ */
/* Systeme simule                                                     */
/* ------------------------------------------------------------------ */
enum { APP_FINDER = 1, APP_WRITE, APP_PAINT, APP_CONTROL, APP_ABOUT, APP_ALERT, APP_DISK, APP_TEL };

#define MAX_WIN 8
#define WIN_TITLE_MAX 28

typedef struct {
    int  id, kind, x, y, w, h;
    char title[WIN_TITLE_MAX];
    int  active, visible;
    void *st;              /* etat propre a l'appli */
} win_t;

typedef struct {
    win_t w[MAX_WIN];
    int   n;
    int   z[MAX_WIN];      /* ordre d'empilement, z[0] = dessous */
    int   next_id;
    int   next_y;          /* placement en cascade */
} wm_t;

void wm_init(wm_t *m);
win_t *wm_open(wm_t *m, int kind, const char *title, int x, int y, int w, int h);
win_t *wm_by_id(wm_t *m, int id);
void wm_close(wm_t *m, int id);
void wm_raise(wm_t *m, int id);
win_t *wm_top_visible(wm_t *m);
win_t *wm_at(wm_t *m, int x, int y);
void wm_lower_all(wm_t *m);

/* menu deroulant */
typedef struct {
    int   open;          /* index du menu ouvert, -1 = aucun */
    int   item;          /* item survole */
} menu_t;

extern const char *MENU_TITRES[5];
int  menu_nitems(int m);
const char *menu_item(int m, int i);
bool menu_item_checked(int m, int i);
bool menu_item_disabled(int m, int i);

/* etat global du systeme simule */
typedef struct {
    disk_t  disk;
    wm_t    wm;
    menu_t  menu;
    int     ecran_allume;      /* 0 = eteint */
    int     boot_phase;        /* 0 = echec disquette, 1 = happy mac, 2 = bureau */
    int64_t boot_ms;
    int     horloge;           /* 1 = horloge dans la barre de menus */
    int     curseur_lu;        /* position du pointeur ecran */
    int     curseur_x, curseur_y;
    int     bouton_enfonce;
    int     selection_macpaint;/* outil courant */
    int     bright;            /* 0..100 : luminosite du tube */
    int     son;               /* 0..100 */
    int     drap;              /* 1 = un deplacement d'icone est en cours */
    char    statut[64];        /* message de pied de bureau */
    int     statut_ms;
    int     annee;             /* annee simulee (1986 par defaut) */
    int     rapide;            /* 1 = demarrage accelere (tests) */
    char    host[64], kernel[128];
    unsigned long ram_ko;
    double  uptime_s, load1;
    int     fenetres_ouvertes;
} sys_t;

void sys_init(sys_t *s);
void sys_tick(sys_t *s, int64_t now_ms);
extern sys_t G;

/* apps */
void app_open(sys_t *s, int kind);
void app_draw(win_t *w, fb_t *f);
void app_click(sys_t *s, win_t *w, int lx, int ly, int double_click);
void app_key(sys_t *s, win_t *w, unsigned key, const char *txt);
void app_close(win_t *w);
void app_render(sys_t *s, fb_t *f);      /* tout l'ecran : bureau + fenetres */
void app_click_screen(sys_t *s, int x, int y, int double_click);
void app_key_screen(sys_t *s, unsigned key, const char *txt);
void app_hover(sys_t *s, int x, int y);
void app_release(win_t *w);
void app_drag(win_t *w, int lx, int ly);
void scene_write_fill(win_t *w);
void scene_paint_demo(win_t *w);
int  app_debug_len(win_t *w);
void app_menu(sys_t *s, int menu, int item);
void app_boot_draw(sys_t *s, fb_t *f, int64_t now_ms);

/* ui (la machine autour de l'ecran) */
typedef struct { double x, y, w, h; } rect_t;
typedef struct {
    double w, h;             /* taille de la fenetre */
    rect_t ecran;            /* zone de l'ecran dans la fenetre */
    rect_t boitier;
    rect_t lecteur;          /* fente disquette */
    rect_t clavier;
    rect_t souris;
    double echelle;          /* 1 px ecran -> echelle px fenetre */
} chrome_t;

void ui_layout(chrome_t *c, double ww, double wh);
void ui_map_window_to_screen(const chrome_t *c, double wx, double wy, int *sx, int *sy);
void ui_draw(cairo_t *cr, const chrome_t *c, fb_t *ecran, sys_t *s, int64_t now_ms);
int  ui_hit_key(const chrome_t *c, double wx, double wy, char *out);
int  ui_hit_mouse_button(const chrome_t *c, double wx, double wy);

#endif /* OLDMAC_H */
