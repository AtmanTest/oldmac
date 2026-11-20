/*  main.c — fenetre GTK4, clavier/souris reels, modes hors ecran
 *  (--shot, --info, --bench, --scene, --t) pour verifier sans afficheur.
 */
#include "oldmac.h"
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static GtkWidget   *da;
static chrome_t     chrome;
static fb_t         ecran;
static int64_t      t0;
static int64_t      sim_ms;          /* temps simule (mode hors ecran) */
static int          drag_win;        /* id de la fenetre deplacee */
static int          drag_dx, drag_dy;
static int          press_screen;
static int          last_click_ms;
static int          last_click_x, last_click_y;
static int          opt_scene = -1;
static const char  *opt_port;        /* le port de la carte BATMino */

static int64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int64_t horloge(void)
{
    return sim_ms > 0 ? t0 + sim_ms : now_ms();
}

/* ------------------------------------------------------------------ */
/* scenes hors ecran                                                  */
/* ------------------------------------------------------------------ */
static void scene_setup(sys_t *s, int scene)
{
    s->rapide = 1;
    s->boot_phase = 2;
    switch (scene) {
    case 0: break;                                  /* bureau nu */
    case 1: app_open(s, APP_FINDER); break;
    case 2: {                                       /* MacWrite, le mot de la photo */
        app_open(s, APP_WRITE);
        scene_write_fill(wm_top_visible(&s->wm));
        break;
    }
    case 3: app_open(s, APP_PAINT); scene_paint_demo(wm_top_visible(&s->wm)); break;
    case 4: app_open(s, APP_CONTROL); break;
    case 5: app_open(s, APP_ABOUT); break;
    case 6: {                                       /* bureau charge, plusieurs fenetres */
        app_open(s, APP_FINDER);
        app_open(s, APP_WRITE);
        app_open(s, APP_PAINT);
        break;
    }
    case 7: app_open(s, APP_TEL); break;            /* la telecommande BATMino */
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* la liaison avec la carte                                           */
/* ------------------------------------------------------------------ */
static gboolean tel_cb(gpointer d)
{
    (void)d;
    if (tel_poll(&G_TEL, now_ms()))
        gtk_widget_queue_draw(GTK_WIDGET(da));
    return G_SOURCE_CONTINUE;
}

/* hors ecran : on laisse la carte repondre pour de vrai avant la capture */
static void servir_telecommande(int max_ms)
{
    int64_t fin = now_ms() + max_ms;
    while (now_ms() < fin) {
        tel_poll(&G_TEL, now_ms());
        if (G_TEL.ecran_lu && G_TEL.etat_lu) break;
        g_usleep(20000);
    }
    G.statut[0] = 0;               /* pas de message de service sur la photo */
}

/* ------------------------------------------------------------------ */
/* dessin                                                            */
/* ------------------------------------------------------------------ */
static void dessine(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data)
{
    (void)area; (void)data;
    sys_t *s = &G;
    ui_layout(&chrome, w, h);
    sys_tick(s, horloge());
    app_render(s, &ecran);
    ui_draw(cr, &chrome, &ecran, s, horloge());
}

static gboolean tick_cb(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
    (void)clock; (void)data;
    gtk_widget_queue_draw(widget);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ */
/* entrees                                                           */
/* ------------------------------------------------------------------ */
static void envoie_touche(unsigned key, const char *txt)
{
    if (key == 0xff08) app_key_screen(&G, 0xff08, NULL);
    else if (key == 0xff1b) app_key_screen(&G, 0xff1b, NULL);
    else if (txt && txt[0]) app_key_screen(&G, key, txt);
}

static void clic_ecran(int x, int y, int dbl)
{
    app_click_screen(&G, x, y, dbl);
}

static void on_press(GtkGestureClick *g, int n, double x, double y, gpointer d)
{
    (void)g; (void)d;
    int sx, sy;
    if (x >= chrome.ecran.x && x < chrome.ecran.x + chrome.ecran.w &&
        y >= chrome.ecran.y && y < chrome.ecran.y + chrome.ecran.h) {
        ui_map_window_to_screen(&chrome, x, y, &sx, &sy);
        G.curseur_x = sx; G.curseur_y = sy;
        int64_t t = horloge();
        int dbl = (t - last_click_ms < 420) &&
                  abs(sx - last_click_x) < 6 && abs(sy - last_click_y) < 6 && n >= 2;
        last_click_ms = (int)t; last_click_x = sx; last_click_y = sy;
        press_screen = 1;
        G.bouton_enfonce = 1;
        /* attrape une fenetre par sa barre de titre pour la deplacer */
        drag_win = 0;
        win_t *w = wm_at(&G.wm, sx, sy);
        if (w && sy - w->y >= 3 && sy - w->y < 19 && sx - w->x > 16 &&
            sx < w->x + w->w - 16) {
            drag_win = w->id;
            drag_dx = sx - w->x;
            drag_dy = sy - w->y;
        }
        clic_ecran(sx, sy, dbl);
        gtk_widget_queue_draw(GTK_WIDGET(da));
        return;
    }
    /* touche du clavier dessine */
    char c = 0;
    int sp = ui_hit_key(&chrome, x, y, &c);
    if (sp) {
        if (sp == 2) envoie_touche(0xff0d, "\n");
        else if (sp == 3) envoie_touche(' ', " ");
        else if (sp == 4) envoie_touche(0xff09, NULL);
        gtk_widget_queue_draw(GTK_WIDGET(da));
        return;
    }
    /* clic sur la souris dessinee : clique a la position du pointeur */
    if (ui_hit_mouse_button(&chrome, x, y)) {
        clic_ecran(G.curseur_x, G.curseur_y, 0);
        gtk_widget_queue_draw(GTK_WIDGET(da));
    }
}

static void on_release(GtkGestureClick *g, int n, double x, double y, gpointer d)
{
    (void)g; (void)n; (void)x; (void)y; (void)d;
    G.bouton_enfonce = 0;
    drag_win = 0;
    press_screen = 0;
    /* fin de trace dans MacPaint */
    win_t *w = wm_top_visible(&G.wm);
    if (w && w->kind == APP_PAINT) app_release(w);
}

static void on_motion(GtkEventControllerMotion *c, double x, double y, gpointer d)
{
    (void)c; (void)d;
    int sx, sy;
    if (x >= chrome.ecran.x && x < chrome.ecran.x + chrome.ecran.w &&
        y >= chrome.ecran.y && y < chrome.ecran.y + chrome.ecran.h) {
        ui_map_window_to_screen(&chrome, x, y, &sx, &sy);
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx > SCR_W - 1) sx = SCR_W - 1;
        if (sy > SCR_H - 1) sy = SCR_H - 1;
        G.curseur_x = sx;
        G.curseur_y = sy;
        G.curseur_lu = 1;
        if (G.bouton_enfonce) {
            win_t *w = wm_top_visible(&G.wm);
            if (w && w->kind == APP_PAINT) app_drag(w, sx - w->x, sy - w->y);
            else if (w && drag_win == w->id) {
                w->x = sx - drag_dx;
                w->y = sy - drag_dy;
                if (w->y < 20) w->y = 20;
            }
        } else {
            app_hover(&G, sx, sy);
        }
        gtk_widget_queue_draw(GTK_WIDGET(da));
    }
}

static gboolean on_key(GtkEventControllerKey *c, guint keyval, guint code,
                       GdkModifierType state, gpointer d)
{
    (void)c; (void)code; (void)d;
    const char *txt = NULL;
    char buf[8] = {0};
    if (keyval == GDK_KEY_BackSpace) { envoie_touche(0xff08, NULL); }
    else if (keyval == GDK_KEY_Escape) { envoie_touche(0xff1b, NULL); }
    else if (keyval == GDK_KEY_Return) { envoie_touche(0xff0d, "\n"); }
    else if (keyval == GDK_KEY_space) { envoie_touche(' ', " "); }
    else if (keyval == GDK_KEY_Left) { envoie_touche(0xff51, NULL); }
    else if (keyval == GDK_KEY_Right) { envoie_touche(0xff53, NULL); }
    else {
        guint u = gdk_keyval_to_unicode(keyval);
        if (u >= 32 && u < 127) {
            buf[0] = (char)u;
            txt = buf;
            envoie_touche(keyval, txt);
        }
    }
    (void)state;
    gtk_widget_queue_draw(GTK_WIDGET(da));
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* hors ecran                                                        */
/* ------------------------------------------------------------------ */
static void rendre_png(const char *path, int ww, int wh, sys_t *s)
{
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ww, wh);
    cairo_t *cr = cairo_create(surf);
    ui_layout(&chrome, ww, wh);
    app_render(s, &ecran);
    ui_draw(cr, &chrome, &ecran, s, horloge());
    cairo_surface_flush(surf);
    cairo_surface_write_to_png(surf, path);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* un fond d'ecran : l'ecran du Mac, sans boitier, en noir et blanc pur,
   agrandi au pixel le plus proche. La trame du bureau continue jusqu'aux
   bords, pour qu'on ne voie pas de bande blanche autour. */
static void rendre_fond(const char *path, int ww, int wh, sys_t *s, int zoom)
{
    (void)zoom;
    if (ww < 320) ww = 320;
    if (wh < 200) wh = 200;
    s->curseur_lu = 0;                        /* pas de pointeur sur un fond */
    s->statut[0] = 0;                         /* ni de message de service  */
    sys_tick(s, horloge());
    app_render(s, &ecran);

    int e = ww / SCR_W, eh = wh / SCR_H;
    if (eh < e) e = eh;
    if (e < 1) e = 1;
    int sw = SCR_W * e, sh = SCR_H * e;
    int ox = (ww - sw) / 2, oy = (wh - sh) / 2;
    /* la trame a une periode de 2e : on cale le coin pour qu'elle continue */
    ox -= ((ox % (2 * e)) + 2 * e) % (2 * e);
    oy -= ((oy % (2 * e)) + 2 * e) % (2 * e);

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ww, wh);
    cairo_surface_flush(surf);
    unsigned char *d = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);

    for (int Y = 0; Y < wh; Y++) {
        uint32_t *ligne = (uint32_t *)(d + (size_t)Y * stride);
        int gy = Y - oy;
        int sy = gy >= 0 ? gy / e : -1;
        for (int X = 0; X < ww; X++) {
            int gx = X - ox;
            int sx = gx >= 0 ? gx / e : -1;
            int noir;
            if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
                noir = fb_pget(&ecran, sx, sy);
            else {
                /* la meme trame que le bureau : un point tous les 2 px */
                int px = gx >= 0 ? gx / e : -(((-gx) + e - 1) / e);
                int py = gy >= 0 ? gy / e : -(((-gy) + e - 1) / e);
                noir = (((px % 2) + 2) % 2 == 0) && (((py % 2) + 2) % 2 == 0);
            }
            ligne[X] = noir ? 0xFF000000u : 0xFFFFFFFFu;
        }
    }
    cairo_surface_mark_dirty(surf);
    cairo_surface_write_to_png(surf, path);
    cairo_surface_destroy(surf);
}

static int mode_info(sys_t *s)
{
    printf("oldmac %s\n", VERSION_STR);
    printf("  ecran        : %dx%d en 1 bit, echelle %.2f\n", SCR_W, SCR_H,
           chrome.echelle > 0 ? chrome.echelle : 1.0);
    printf("  disque       : %s\n", s->disk.root);
    printf("  documents    : %d, corbeille %d, %d Ko libres sur %d\n",
           s->disk.n, s->disk.trash_n, disk_free_ko(), disk_capacity_ko());
    printf("  fenetres     : %d\n", s->wm.n);
    printf("  machine      : %s, %s, %lu Mo, charge %.2f, en marche %d min\n",
           s->host, s->kernel, s->ram_ko / 1024, s->load1, (int)(s->uptime_s / 60));
    printf("  luminosite   : %d %%, son %d %%\n", s->bright, s->son);
    printf("  BATMino      : %s, %s\n", G_TEL.port, G_TEL.status);
    return 0;
}

static int mode_bench(sys_t *s, int iterations)
{
    int ww = 900, wh = 954;
    ui_layout(&chrome, ww, wh);
    int64_t start = now_ms();
    for (int i = 0; i < iterations; i++) {
        sys_tick(s, now_ms());
        app_render(s, &ecran);
    }
    int64_t dt = now_ms() - start;
    double par = dt / (double)iterations;
    printf("oldmac %s — banc\n", VERSION_STR);
    printf("  %d images simulees en %lld ms (%.2f ms/image, %.0f i/s)\n",
           iterations, (long long)dt, par, par > 0 ? 1000.0 / par : 0);
    printf("  pixels allumes a l'ecran : %d / %d (%.1f %%)\n",
           fb_ink(&ecran, 0, 0, SCR_W, SCR_H), SCR_W * SCR_H,
           100.0 * fb_ink(&ecran, 0, 0, SCR_W, SCR_H) / (SCR_W * SCR_H));
    return 0;
}

static void activer(GtkApplication *app, gpointer data)
{
    (void)data;
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "oldmac — Macintosh Plus simule");
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 954);
    da = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da), dessine, NULL, NULL);
    gtk_window_set_child(GTK_WINDOW(win), da);

    GtkEventController *m = gtk_event_controller_motion_new();
    g_signal_connect(m, "motion", G_CALLBACK(on_motion), NULL);
    gtk_widget_add_controller(da, m);

    GtkGesture *clic = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(clic), GDK_BUTTON_PRIMARY);
    g_signal_connect(clic, "pressed", G_CALLBACK(on_press), NULL);
    g_signal_connect(clic, "released", G_CALLBACK(on_release), NULL);
    gtk_widget_add_controller(da, GTK_EVENT_CONTROLLER(clic));

    GtkEventController *kb = gtk_event_controller_key_new();
    g_signal_connect(kb, "key-pressed", G_CALLBACK(on_key), NULL);
    gtk_widget_add_controller(win, kb);

    gtk_widget_add_tick_callback(da, tick_cb, NULL, NULL);
    g_timeout_add(150, tel_cb, NULL);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv)
{
    const char *shot = NULL, *fond = NULL;
    int bench = 0, info = 0, zoom = 5, fw = 2560, fh = 1440;
    double t_sim = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot = argv[++i];
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) opt_scene = atoi(argv[++i]);
        else if (strcmp(argv[i], "--t") == 0 && i + 1 < argc) t_sim = atof(argv[++i]);
        else if (strcmp(argv[i], "--bench") == 0) bench = 1;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) opt_port = argv[++i];
        else if (strcmp(argv[i], "--fond") == 0 && i + 1 < argc) fond = argv[++i];
        else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) zoom = atoi(argv[++i]);
        else if (strcmp(argv[i], "--taille") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &fw, &fh) != 2) { fw = 2560; fh = 1440; }
        }
        else if (strcmp(argv[i], "--info") == 0) info = 1;
        else if (strcmp(argv[i], "--version") == 0) { printf("oldmac %s\n", VERSION_STR); return 0; }
        else if (strcmp(argv[i], "--aide") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("oldmac %s — Macintosh compact simule\n"
                   "  (sans option)     fenetre GTK4, clavier et souris reels\n"
                   "  --shot FICHIER    capture PNG de la machine\n"
                   "  --scene N         0 bureau, 1 Finder, 2 MacWrite, 3 MacPaint,\n"
                   "                    4 tableau de bord, 5 a propos, 6 trois fenetres,\n"
                   "                    7 telecommande BATMino\n"
                   "  --port PERIPH     le port de la carte (defaut " TEL_PORT_DEFAUT ")\n"
                   "  --t SECONDES      avance le temps simule avant la capture\n"
                   "  --fond FICHIER    fond d'ecran : l'ecran du Mac en noir et blanc\n"
                   "  --taille LxH      taille du fond (defaut 2560x1440)\n"
                   "  --zoom N          echelle forcee du fond (inutile : il s'adapte)\n"
                   "  --info            etat du systeme simule\n"
                   "  --bench           vitesse de rendu\n", VERSION_STR);
            return 0;
        }
    }

    t0 = now_ms();
    fb_init(&ecran, SCR_W, SCR_H);
    sys_init(&G);
    tel_init(&G_TEL, opt_port ? opt_port : TEL_PORT_DEFAUT);

    if (shot || info || bench || fond) {
        sim_ms = t_sim > 0 ? (int64_t)(t_sim * 1000) : 4000;
        if (opt_scene >= 0) scene_setup(&G, opt_scene);
        if (fond) {
            if (opt_scene == 7) servir_telecommande(2500);
            rendre_fond(fond, fw, fh, &G, zoom);
            printf("fond d'ecran : %s (%dx%d)\n", fond, fw, fh);
            return 0;
        }
        sys_tick(&G, horloge());
        if (info) { ui_layout(&chrome, 900, 954); mode_info(&G); }
        else if (bench) {
            G.rapide = 1;
            sys_tick(&G, horloge());
            G.boot_phase = 2;
            mode_bench(&G, 300);
        }
        else {
            if (opt_scene == 7) servir_telecommande(2500);
            sys_tick(&G, horloge());
            rendre_png(shot, 900, 954, &G);
            printf("capture : %s\n", shot);
        }
        return 0;
    }

    GtkApplication *app = gtk_application_new("org.taz.oldmac", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activer), NULL);
    int st = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    fb_free(&ecran);
    return st;
}
