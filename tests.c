/*  tests.c — verifications du Macintosh simule, sans afficheur.
 *  On ne teste pas des details internes : on ouvre des fenetres, on tape,
 *  on clique, on enregistre, et on regarde le disque et l'ecran 1 bit.
 */
#include "oldmac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>

static int checks, fails;
static const char *groupe = "";

static void titre(const char *g)
{
    groupe = g;
    printf("— %s\n", g);
}

#define CHECK(cond, ...) do {                                  \
    checks++;                                                  \
    if (!(cond)) {                                             \
        fails++;                                               \
        printf("  ECHEC [%s] ", groupe);                       \
        printf(__VA_ARGS__);                                   \
        printf("\n");                                          \
    }                                                          \
} while (0)

static fb_t ecran;
static int64_t t;

static void passe(int64_t dt)
{
    t += dt;
    sys_tick(&G, t);
    app_render(&G, &ecran);
}

/* ------------------------------------------------------------------ */
static void test_fb(void)
{
    titre("framebuffer 1 bit");
    fb_t f;
    fb_init(&f, 64, 32);
    CHECK(fb_ink(&f, 0, 0, 64, 32) == 0, "un framebuffer neuf doit etre blanc");
    fb_rect(&f, 10, 10, 20, 5, 1);
    CHECK(fb_ink(&f, 10, 10, 20, 5) == 100, "rectangle 20x5 = 100 pixels noirs");
    fb_pset(&f, 1000, 1000, 1);
    CHECK(fb_pget(&f, 1000, 1000) == 0, "on ne doit pas ecrire hors du cadre");
    fb_invert(&f, 10, 10, 20, 5);
    CHECK(fb_ink(&f, 10, 10, 20, 5) == 0, "inverser deux fois revient au blanc");
    fb_gray(&f, 0, 0, 10, 10);
    int g = fb_ink(&f, 0, 0, 10, 10);
    CHECK(g == 25, "trame 50 %% : 25 pixels sur 100 (obtenu %d)", g);
    fb_clear(&f);
    fb_line(&f, 0, 0, 63, 31, 1);
    CHECK(fb_pget(&f, 0, 0) == 1 && fb_pget(&f, 63, 31) == 1,
          "une ligne doit toucher ses deux extremites");
    CHECK(fb_ink(&f, 0, 0, 64, 32) >= 32, "une ligne doit etre continue");
    fb_free(&f);
}

static void test_texte(void)
{
    titre("texte en 1 bit");
    int w = tx_width("Macintosh", 11, TF_NORMAL);
    CHECK(w > 30 && w < 200, "largeur de \"Macintosh\" plausible (%d px)", w);
    CHECK(tx_width("", 11, TF_NORMAL) == 0, "une chaine vide n'a pas de largeur");
    CHECK(tx_height(11, TF_NORMAL) >= 8 && tx_height(11, TF_NORMAL) <= 20,
          "hauteur de ligne plausible (%d px)", tx_height(11, TF_NORMAL));

    fb_t f;
    fb_init(&f, 200, 40);
    tx_draw(&f, 5, 5, "Bonjour", 12, TF_BOLD);
    int ink = fb_ink(&f, 0, 0, 200, 40);
    CHECK(ink > 20, "du texte doit laisser de l'encre (%d pixels)", ink);
    CHECK(ink < 200 * 40 / 3, "du texte ne doit pas remplir la page (%d)", ink);
    /* un texte plus long laisse plus d'encre */
    fb_clear(&f);
    tx_draw(&f, 5, 5, "Bonjour Bonjour Bonjour", 12, TF_BOLD);
    int ink2 = fb_ink(&f, 0, 0, 200, 40);
    CHECK(ink2 > ink, "un texte plus long doit laisser plus d'encre");
    fb_free(&f);
}

static void test_disque(void)
{
    titre("disque virtuel");
    disk_t d;
    disk_init(&d);
    CHECK(d.root[0] == '/', "le disque doit avoir un chemin absolu : %s", d.root);

    CHECK(disk_write_text(&d, "Lettre a papa", "bonjour"), "ecriture d'un texte");
    disk_rescan(&d);
    int trouve = 0;
    for (int i = 0; i < d.n; i++)
        if (strcmp(d.f[i].name, "Lettre a papa") == 0 && d.f[i].type == FT_TEXT) trouve = 1;
    CHECK(trouve, "le document doit apparaitre apres relecture");

    int len = 0;
    char *txt = disk_read(&d, "Lettre a papa", &len);
    CHECK(txt && len == 7 && strcmp(txt, "bonjour") == 0, "relire le document");
    free(txt);

    uint8_t bits[6 * 4];
    for (int i = 0; i < 24; i++) bits[i] = (uint8_t)(i % 2);
    CHECK(disk_write_paint(&d, "croquis", bits, 6, 4), "ecrire un dessin PBM");
    int w = 0, h = 0;
    uint8_t *rb = disk_read_paint(&d, "croquis", &w, &h);
    CHECK(rb && w == 6 && h == 4, "relire le dessin (%dx%d)", w, h);
    CHECK(rb && rb[0] == 0 && rb[1] == 1, "les pixels doivent etre conserves");
    free(rb);

    int avant = d.n;
    disk_trash(&d, "Lettre a papa");
    CHECK(d.n == avant - 1, "mettre a la corbeille retire du disque");
    CHECK(d.trash_n == 1, "un document doit arriver dans la corbeille");
    disk_empty_trash(&d);
    CHECK(d.trash_n == 0, "vider la corbeille la vide");
    CHECK(disk_free_ko() > 0 && disk_free_ko() <= disk_capacity_ko(),
          "espace libre coherent (%d Ko)", disk_free_ko());
}

static void test_fenetres(void)
{
    titre("fenetres");
    wm_init(&G.wm);
    win_t *a = wm_open(&G.wm, APP_WRITE, "A", 10, 30, 200, 120);
    win_t *b = wm_open(&G.wm, APP_WRITE, "B", 40, 50, 200, 120);
    CHECK(G.wm.n == 2, "deux fenetres ouvertes");
    CHECK(wm_at(&G.wm, 45, 55) == b, "le clic doit trouver la fenetre du dessus");
    wm_raise(&G.wm, a->id);
    CHECK(wm_at(&G.wm, 45, 55) == a, "apres raise, la fenetre A est devant");
    CHECK(a->active == 1 && b->active == 0, "une seule fenetre active");
    int id = a->id;
    int idb = b->id;
    wm_close(&G.wm, id);
    CHECK(G.wm.n == 1 && wm_by_id(&G.wm, id) == NULL, "fermer retire la fenetre");
    win_t *reste = wm_top_visible(&G.wm);
    CHECK(reste && reste->id == idb, "la fenetre restante devient active");
    wm_close(&G.wm, idb);
    CHECK(G.wm.n == 0, "plus de fenetre : bureau nu");
}

static void test_demarrage(void)
{
    titre("demarrage");
    G.boot_phase = 0;
    G.rapide = 0;
    G.boot_ms = t;
    fb_clear(&ecran);
    app_boot_draw(&G, &ecran, t + 300);
    CHECK(fb_ink(&ecran, 0, 0, SCR_W, SCR_H) > 0, "phase 1 : la disquette s'affiche");
    app_boot_draw(&G, &ecran, t + 1500);
    CHECK(fb_ink(&ecran, 0, 0, SCR_W, SCR_H) > 0, "phase 2 : le Happy Mac s'affiche");
    app_boot_draw(&G, &ecran, t + 4000);
    int barre = fb_ink(&ecran, 0, 19, SCR_W, 1);
    CHECK(barre > SCR_W / 2, "phase 3 : la barre de menus est tracee (%d)", barre);
    G.rapide = 1;
}

static void test_bureau(void)
{
    titre("bureau");
    G.boot_phase = 2;
    fb_clear(&ecran);
    app_render(&G, &ecran);
    int barre = fb_ink(&ecran, 0, 19, SCR_W, 1);
    CHECK(barre == SCR_W, "le trait sous la barre de menus fait toute la largeur");
    CHECK(fb_ink(&ecran, 4, 4, 14, 14) > 10, "la pomme est dessinee en haut a gauche");
    int trame = fb_ink(&ecran, 300, 250, 20, 20);
    CHECK(trame > 50 && trame < 320, "le bureau est en trame grise (%d/400)", trame);
    CHECK(fb_ink(&ecran, 26, 42, 32, 30) > 20, "l'icone du disque est dessinee");
}

static void test_finder(void)
{
    titre("finder");
    G.wm.n = 0;
    app_open(&G, APP_FINDER);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_FINDER, "le Finder s'ouvre");
    CHECK(w->w > 200 && w->h > 120, "le Finder a une taille utilisable");
    passe(16);
    int encre = fb_ink(&ecran, w->x + 2, w->y + 20, w->w - 4, 20);
    CHECK(encre > 30, "le Finder ecrit son entete (%d)", encre);
    CHECK(G.disk.n >= 0, "le disque est lisible depuis le Finder");

    /* ouvrir un document existant par double clic */
    disk_t *d = &G.disk;
    disk_write_text(d, "note", "du texte");
    disk_rescan(d);
    int ligne = -1;
    for (int i = 0; i < d->n; i++) if (strcmp(d->f[i].name, "note") == 0) ligne = i + 1;
    CHECK(ligne > 0, "le document cree doit etre dans la liste");
    if (ligne > 0) {
        int avant = G.wm.n;
        app_click_screen(&G, w->x + 30, w->y + 19 + 1 + 14 + ligne * 16 + 3, 1);
        CHECK(G.wm.n == avant + 1, "double clic : le document doit s'ouvrir");
        win_t *nw = wm_top_visible(&G.wm);
        CHECK(nw && nw->kind == APP_WRITE, "il s'ouvre dans MacWrite");
        if (nw) wm_close(&G.wm, nw->id);
    }
    wm_close(&G.wm, w->id);
}

static void test_macwrite(void)
{
    titre("macwrite");
    G.wm.n = 0;
    app_open(&G, APP_WRITE);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_WRITE, "MacWrite s'ouvre");

    /* taper du texte au clavier */
    const char *mot = "bonjour";
    for (int i = 0; mot[i]; i++) {
        char b[2] = { mot[i], 0 };
        app_key_screen(&G, (unsigned char)mot[i], b);
    }
    passe(16);
    CHECK(fb_ink(&ecran, w->x + 4, w->y + 20, w->w - 8, 40) > 40,
          "le texte tape doit s'afficher");

    /* effacer une lettre, puis enregistrer */
    app_key_screen(&G, 0xff08, NULL);
    app_menu(&G, 1, 3);
    CHECK(G.disk.n >= 1, "enregistrer doit creer un document");
    int trouve = 0;
    for (int i = 0; i < G.disk.n; i++) {
        int len = 0;
        char *c = disk_read(&G.disk, G.disk.f[i].name, &len);
        if (c && strcmp(c, "bonjou") == 0) trouve = 1;
        free(c);
    }
    CHECK(trouve, "le document enregistre contient le texte tape (moins la lettre effacee)");

    /* tout effacer */
    app_menu(&G, 2, 5);
    passe(16);
    int encre = fb_ink(&ecran, w->x + 6, w->y + 24, w->w - 12, 40);
    CHECK(encre < 40, "Tout effacer vide le document (%d)", encre);
    wm_close(&G.wm, w->id);
    CHECK(G.wm.n == 0, "fermer MacWrite");
}

static void test_macpaint(void)
{
    titre("macpaint");
    G.wm.n = 0;
    app_open(&G, APP_PAINT);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_PAINT, "MacPaint s'ouvre");

    /* le dessin de demonstration */
    scene_paint_demo(w);
    passe(16);
    int avant = fb_ink(&ecran, w->x + 45, w->y + 25, w->w - 50, w->h - 45);
    CHECK(avant > 300, "le dessin de demonstration laisse de l'encre (%d)", avant);

    /* dessiner au crayon : un trait franc a travers la zone */
    int cx = w->x + 44 + 10;
    int cy = w->y + 19 + 60;
    app_click(&G, w, cx, cy, 0);
    app_drag(w, cx + 40, cy + 10);
    app_release(w);
    passe(16);
    int apres = fb_ink(&ecran, w->x + 45, w->y + 25, w->w - 50, w->h - 45);
    CHECK(apres > avant, "le crayon doit ajouter de l'encre (%d -> %d)", avant, apres);

    /* enregistrer le dessin : un vrai fichier PBM sur le disque */
    int avant_n = G.disk.n;
    app_menu(&G, 1, 3);
    CHECK(G.disk.n == avant_n + 1, "enregistrer un dessin ajoute un document");
    int trouve = 0;
    for (int i = 0; i < G.disk.n; i++)
        if (G.disk.f[i].type == FT_PAINT) trouve = 1;
    CHECK(trouve, "le dessin doit etre de type peinture (PBM)");
    wm_close(&G.wm, w->id);
}

static void test_menus(void)
{
    titre("menus");
    G.wm.n = 0;
    app_open(&G, APP_WRITE);
    G.menu.open = -1;
    G.menu.item = -1;

    /* cliquer sur le titre "Fichier" ouvre le menu */
    app_click_screen(&G, 40, 8, 0);
    CHECK(G.menu.open >= 0, "un clic dans la barre ouvre un menu (%d)", G.menu.open);
    G.menu.open = 1;
    app_hover(&G, 60, 20 + 1 + 16 * 3 + 4);
    CHECK(G.menu.item == 3, "survoler un item le selectionne (%d)", G.menu.item);
    app_click_screen(&G, 60, 20 + 1 + 16 * 3 + 4, 0);
    CHECK(G.menu.open == -1, "choisir un item referme le menu");

    CHECK(menu_nitems(0) == 10, "le menu Apple a 10 entrees");
    CHECK(menu_item(1, 4) && strcmp(menu_item(1, 4), "--") == 0,
          "une entree peut etre un separateur");
    CHECK(menu_item_disabled(1, 4), "un separateur est inactif");

    /* l'horloge se bascule par le menu Apple */
    int h = G.horloge;
    app_menu(&G, 0, 2);
    CHECK(G.horloge != h, "le menu Apple bascule l'horloge");
    app_menu(&G, 0, 2);
    CHECK(G.horloge == h, "et la remet comme avant");

    /* eteindre, puis rallumer d'un clic */
    app_menu(&G, 0, 9);
    CHECK(G.ecran_allume == 0, "Eteindre eteint l'ecran");
    passe(16);
    CHECK(fb_ink(&ecran, 0, 0, SCR_W, SCR_H) > SCR_W * SCR_H / 2,
          "ecran eteint : la dalle est noire");
    app_click_screen(&G, 200, 100, 0);
    CHECK(G.ecran_allume == 1, "un clic rallume la machine");
    CHECK(G.boot_phase == 0, "rallumer relance le demarrage");
    G.rapide = 1;
    passe(16);
    CHECK(G.boot_phase == 2, "le demarrage accelere rend le bureau");
    G.wm.n = 0;
}

static void test_panneaux(void)
{
    titre("tableau de bord et a propos");
    G.wm.n = 0;
    app_open(&G, APP_CONTROL);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_CONTROL, "le tableau de bord s'ouvre");
    int avant = G.bright;
    app_click(&G, w, 20, 19 + 1 + 30, 0);       /* clic tout a gauche du curseur */
    CHECK(G.bright < avant, "cliquer a gauche baisse la luminosite (%d -> %d)", avant, G.bright);
    app_click(&G, w, w->w - 20, 19 + 1 + 30, 0);
    CHECK(G.bright > 90, "cliquer a droite la remonte (%d)", G.bright);
    passe(16);
    CHECK(fb_ink(&ecran, w->x + 10, w->y + 20, w->w - 20, w->h - 30) > 100,
          "le tableau de bord affiche quelque chose");
    wm_close(&G.wm, w->id);

    app_open(&G, APP_ABOUT);
    w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_ABOUT, "la fenetre A propos s'ouvre");
    passe(16);
    CHECK(fb_ink(&ecran, w->x + 10, w->y + 20, w->w - 20, w->h - 30) > 100,
          "la fenetre A propos affiche quelque chose");
    wm_close(&G.wm, w->id);
}

static void test_clavier_machine(void)
{
    titre("clavier et souris de la machine");
    chrome_t c;
    ui_layout(&c, 900, 954);
    CHECK(c.echelle > 0.5 && c.echelle < 1.5, "echelle du boitier plausible (%.2f)",
          c.echelle);
    CHECK(c.ecran.w > 400, "l'ecran doit occuper la largeur du boitier (%.0f px)",
          c.ecran.w);

    /* une touche du clavier dessine renvoie bien un caractere */
    {
        char out = 0;
        int hit = ui_hit_key(&c, c.clavier.x + 10, c.clavier.y + 10, &out);
        CHECK(hit != 0, "cliquer sur le clavier doit toucher une touche");
        char out2 = 0;
        int hit2 = ui_hit_key(&c, c.clavier.x + 160, c.clavier.y + 45, &out2);
        CHECK(hit2 == 10 && out2 >= 'a' && out2 <= 'z',
              "une touche de lettre donne son caractere (%d '%c')", hit2, out2);
    }
    /* le centre de l'ecran correspond au centre du framebuffer */
    int sx = 0, sy = 0;
    ui_map_window_to_screen(&c, c.ecran.x + c.ecran.w / 2, c.ecran.y + c.ecran.h / 2,
                            &sx, &sy);
    CHECK(abs(sx - SCR_W / 2) <= 2 && abs(sy - SCR_H / 2) <= 2,
          "le centre de l'ecran tombe au centre du framebuffer (%d,%d)", sx, sy);
    /* hors de l'ecran : la souris dessinee a son bouton */
    CHECK(ui_hit_mouse_button(&c, c.souris.x + 5, c.souris.y + 5) == 1,
          "le bouton de la souris se clique");
}

static void test_presse_papiers(void)
{
    titre("presse-papiers et corbeille");
    G.wm.n = 0;
    app_open(&G, APP_WRITE);
    (void)0;
    for (int i = 0; i < 5; i++) {
        char b[2] = { "salut"[i], 0 };
        app_key_screen(&G, (unsigned char)b[0], b);
    }
    passe(16);
    CHECK(app_debug_len(wm_top_visible(&G.wm)) == 5, "cinq frappes donnent cinq caracteres");
    app_menu(&G, 2, 3);                 /* copier */
    app_menu(&G, 2, 5);                 /* tout effacer */
    app_menu(&G, 2, 4);                 /* coller */
    app_menu(&G, 1, 3);                 /* enregistrer */
    int trouve = 0;
    for (int i = 0; i < G.disk.n; i++) {
        int len = 0;
        char *c = disk_read(&G.disk, G.disk.f[i].name, &len);
        if (c && strcmp(c, "salut") == 0) trouve = 1;
        free(c);
    }
    CHECK(trouve, "copier, effacer puis coller redonne le meme texte");

    /* mettre le document a la corbeille par le menu Fichier */
    int n = G.disk.n;
    app_menu(&G, 1, 7);
    CHECK(G.disk.n == n - 1, "le menu met le document a la corbeille");
    CHECK(G.wm.n == 0, "et referme la fenetre");
    app_menu(&G, 4, 0);                 /* vider la corbeille */
    CHECK(G.disk.trash_n == 0, "le menu Special vide la corbeille");
}

static void test_persistance(void)
{
    titre("persistance du disque");
    disk_t d2;
    disk_init(&d2);
    CHECK(strcmp(d2.root, G.disk.root) == 0, "le disque garde le meme dossier");
    disk_write_text(&d2, "relu", "contenu durable");
    disk_t d3;
    disk_init(&d3);
    int len = 0;
    char *c = disk_read(&d3, "relu", &len);
    CHECK(c && strcmp(c, "contenu durable") == 0,
          "un document ecrit est relu par une nouvelle session");
    free(c);
    disk_trash(&d3, "relu");
    disk_empty_trash(&d3);
}

static void test_menu_dessine(void)
{
    titre("menu deroulant");
    G.wm.n = 0;
    G.menu.open = 1;
    G.menu.item = 3;
    passe(16);
    /* le menu ouvert pose du blanc la ou le bureau est en trame */
    CHECK(fb_pget(&ecran, 30, 30) == 0, "le menu ouvert recouvre le bureau");
    int ouvert = fb_ink(&ecran, 24, 20, 130, 140);
    CHECK(ouvert > 100, "le menu ouvert doit etre dessine sur l'ecran (%d)", ouvert);
    G.menu.open = -1;
    passe(16);
    CHECK(fb_pget(&ecran, 30, 30) == 1, "menu ferme : le bureau reapparait");
    int ferme = fb_ink(&ecran, 24, 20, 130, 140);
    CHECK(ferme > ouvert, "menu ferme : la trame du bureau est de retour (%d > %d)",
          ferme, ouvert);
}

static void test_robustesse(void)
{
    titre("robustesse");
    G.wm.n = 0;
    app_key_screen(&G, 'a', "a");                  /* aucune fenetre : rien */
    app_click_screen(&G, 500, 330, 1);             /* clic dans le vide */
    app_menu(&G, 1, 3);                            /* enregistrer sans fenetre */
    CHECK(G.wm.n == 0, "sans fenetre, rien ne s'ouvre tout seul");
    app_open(&G, APP_ALERT);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_ALERT, "une alerte peut s'ouvrir");
    app_click(&G, w, 6, 8, 0);                     /* clic sur la case de fermeture */
    CHECK(G.wm.n == 0, "la case de fermeture ferme l'alerte");

    /* le MacPaint ne doit pas ecrire hors de sa toile */
    G.wm.n = 0;
    app_open(&G, APP_PAINT);
    w = wm_top_visible(&G.wm);
    app_click(&G, w, w->w - 3, w->h - 3, 0);
    app_drag(w, w->w + 50, w->h + 50);
    app_release(w);
    CHECK(1, "dessiner hors de la toile ne plante pas");
    wm_close(&G.wm, w->id);
}

static void test_parcours(void)
{
    titre("parcours complet a la souris");
    G.wm.n = 0;
    G.menu.open = -1;
    G.menu.item = -1;
    passe(16);

    /* 1. la pomme, puis MacPaint dans le menu */
    app_click_screen(&G, 10, 8, 0);
    CHECK(G.menu.open == 0, "clic sur la pomme ouvre le menu Apple (%d)", G.menu.open);
    app_hover(&G, 40, MB_POPUP_Y + 5 * 16 + 4);
    CHECK(G.menu.item == 5, "survol de MacPaint (%d)", G.menu.item);
    app_click_screen(&G, 40, MB_POPUP_Y + 5 * 16 + 4, 0);
    win_t *paint = wm_top_visible(&G.wm);
    CHECK(paint && paint->kind == APP_PAINT, "MacPaint s'ouvre par le menu");

    /* 2. dessiner dans la toile : un trait au crayon */
    int cx = paint->x + 44 + 4 + 4;
    int cy = paint->y + 19 + 4 + 40;
    app_click_screen(&G, cx, cy, 0);
    app_click_screen(&G, cx + 30, cy + 6, 0);
    app_hover(&G, cx + 30, cy + 6);
    passe(16);
    CHECK(fb_ink(&ecran, cx, cy - 2, 40, 12) > 5, "le trait apparait sur l'ecran");

    /* 3. enregistrer par le menu Fichier */
    app_click_screen(&G, 60, 8, 0);
    CHECK(G.menu.open == 1, "clic sur Fichier ouvre le menu Fichier (%d)", G.menu.open);
    app_hover(&G, 60, MB_POPUP_Y + 3 * 16 + 4);
    app_click_screen(&G, 60, MB_POPUP_Y + 3 * 16 + 4, 0);
    int peint = 0;
    for (int i = 0; i < G.disk.n; i++) if (G.disk.f[i].type == FT_PAINT) peint = 1;
    CHECK(peint, "le dessin est enregistre sur le disque");
    wm_close(&G.wm, paint->id);

    /* 4. double clic sur l'icone MacWrite du bureau */
    int avant = G.wm.n;
    app_click_screen(&G, 46, 160, 1);
    CHECK(G.wm.n == avant + 1, "double clic sur l'icone ouvre une fenetre");
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_WRITE, "c'est MacWrite qui s'ouvre");
    app_key_screen(&G, 'o', "o");
    app_key_screen(&G, 'k', "k");
    CHECK(app_debug_len(w) == 2, "on peut taper dans la fenetre ouverte");
    wm_close(&G.wm, w->id);

    /* 5. la corbeille : double clic dessus ne fait rien de mechant */
    app_click_screen(&G, 452, 266, 1);
    CHECK(G.wm.n >= 0, "la corbeille repond sans planter");
    for (int i = 0; i < G.wm.n; i++)
        if (G.wm.w[i].kind == APP_ALERT) { wm_close(&G.wm, G.wm.w[i].id); break; }
    CHECK(1, "parcours termine");
}
/* ------------------------------------------------------------------ */
/* la telecommande : un faux cable, le vrai protocole de la carte     */
/* ------------------------------------------------------------------ */
static int lire_cable(int fd, char *out, size_t cap, int ms)
{
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    fd_set r;
    FD_ZERO(&r);
    FD_SET(fd, &r);
    out[0] = 0;
    if (select(fd + 1, &r, NULL, NULL, &tv) <= 0) return 0;
    ssize_t n = read(fd, out, cap - 1);
    if (n < 0) n = 0;
    out[n] = 0;
    return (int)n;
}

/* la trame que la carte enverrait : 5000 octets en hexa */
static size_t trame_hexa(char *dst, size_t cap, const uint8_t *octets)
{
    static const char H[] = "0123456789abcdef";
    if (cap < (size_t)TEL_FB_OCTETS * 2 + 5) return 0;
    memcpy(dst, "FB ", 3);
    for (int i = 0; i < TEL_FB_OCTETS; i++) {
        dst[3 + i * 2] = H[octets[i] >> 4];
        dst[4 + i * 2] = H[octets[i] & 15];
    }
    dst[3 + TEL_FB_OCTETS * 2] = '\n';
    dst[4 + TEL_FB_OCTETS * 2] = 0;
    return 3 + (size_t)TEL_FB_OCTETS * 2 + 1;
}

/* tout ce que le Mac a ecrit sur le cable, pour le relire apres coup */
static char fil[4096];

/* la carte de l'autre bout : elle lit et repond comme le firmware */
static void carte_joue(int fd, const char *js, const char *ligne)
{
    char cmd[256];
    for (;;) {
        int n = lire_cable(fd, cmd, sizeof(cmd), 20);
        if (n <= 0) break;
        strncat(fil, cmd, sizeof(fil) - strlen(fil) - 1);
        if (!strncmp(cmd, "p ", 2)) {
            char r[40];
            snprintf(r, sizeof(r), "page %d\n", atoi(cmd + 2));
            if (write(fd, r, strlen(r)) < 0) { }
        } else if (!strncmp(cmd, "b ", 2)) {
            char r[64];
            snprintf(r, sizeof(r), "bouton %.40s", cmd + 2);
            if (write(fd, r, strlen(r)) < 0) { }
        } else if (cmd[0] == 's') {
            if (write(fd, ligne, strlen(ligne)) < 0) { }
        } else if (cmd[0] == 'e') {
            if (write(fd, js, strlen(js)) < 0) { }
        }
    }
}

static void test_liaison(void)
{
    titre("telecommande BATMino");

    tel_t T;
    tel_init(&T, "faux cable");
    CHECK(strcmp(T.port, "faux cable") == 0, "le port se retient (%s)", T.port);
    CHECK(T.fd < 0 && !T.ouvert, "aucun port ouvert au depart");

    int cable[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, cable) == 0, "un faux cable doit s'ouvrir");
    tel_attacher(&T, cable[0]);
    CHECK(T.ouvert, "le cable est attache");

    /* la carte : son etat, et sa dalle */
    const char *js =
        "{\"page\":3,\"curseur\":2,\"mode\":1,\"items\":9,\"profil\":\"ATMAN\","
        "\"solde_ct\":1250,\"hist\":4,\"top\":2,\"wifi\":true,\"heure\":false,"
        "\"rendu_ms\":180,\"ecran_ms\":900,\"heap\":120000,\"psram\":7000000,"
        "\"version\":\"3.0.0\"}\n";
    uint8_t octets[TEL_FB_OCTETS];
    memset(octets, 0, sizeof(octets));
    octets[0] = 0x80;                       /* le pixel (0,0)    */
    octets[50 * 25 + 12] = 0x08;            /* le pixel (100,50) */
    char ligne[TEL_FB_OCTETS * 2 + 8];
    size_t nl = trame_hexa(ligne, sizeof(ligne), octets);
    CHECK(nl == (size_t)TEL_FB_OCTETS * 2 + 4, "la trame fait 10 004 octets (%zu)", nl);

    /* 1. l'etat JSON */
    CHECK(write(cable[1], js, strlen(js)) == (ssize_t)strlen(js), "la carte ecrit son etat");
    int64_t t0 = 5000;
    tel_poll(&T, t0);
    CHECK(T.etat_lu, "l'etat JSON doit etre lu");
    CHECK(T.page == 3 && T.items == 9 && T.curseur == 2,
          "page %d, %d elements, curseur %d", T.page, T.items, T.curseur);
    CHECK(T.solde_ct == 1250 && T.hist == 4 && T.top == 2,
          "solde %d, historique %d, top %d", T.solde_ct, T.hist, T.top);
    CHECK(strcmp(T.profil, "ATMAN") == 0, "le profil se lit (%s)", T.profil);
    CHECK(T.wifi == 1 && T.heure == 0, "wifi %d, heure %d", T.wifi, T.heure);
    CHECK(strcmp(T.version, "3.0.0") == 0, "la version du firmware (%s)", T.version);
    CHECK(strcmp(T.status, "carte prete") == 0, "la liaison est declaree prete (%s)", T.status);
    CHECK(T.joint, "la carte est jointe");

    /* 2. la dalle, 5000 octets en hexa */
    CHECK(write(cable[1], ligne, nl) == (ssize_t)nl, "la carte envoie sa dalle");
    tel_poll(&T, t0 + 10);
    CHECK(T.ecran_lu, "la trame doit etre decodee");
    CHECK(T.ecran[0] == 0x80, "le premier octet de la dalle (%02x)", T.ecran[0]);
    CHECK((T.ecran[50 * 25 + 12] >> 3) & 1, "le pixel (100,50) est allume");

    /* 3. demander une page : la commande part, la carte confirme */
    fil[0] = 0;
    tel_page(&T, 7);
    tel_poll(&T, t0 + 20);
    CHECK(T.attente == TEL_ATT_PAGE, "une commande est en vol (%d)", T.attente);
    carte_joue(cable[1], js, ligne);
    CHECK(strstr(fil, "p 7\n") != NULL, "la page part sur le cable (%s)", fil);
    tel_poll(&T, t0 + 30);
    CHECK(T.page == 7, "la page confirmee devient la page courante (%d)", T.page);
    /* la confirmation declenche tout de suite la demande d'image */
    CHECK(T.attente == TEL_ATT_ECRAN, "le Mac redemande la dalle aussitot (%d)", T.attente);
    carte_joue(cable[1], js, ligne);
    CHECK(strstr(fil, "s\n") != NULL, "le Mac redemande la dalle");
    tel_poll(&T, t0 + 40);
    CHECK(T.ecran_lu && T.attente == TEL_ATT_RIEN, "la dalle est revenue");

    /* 4. un geste sur le bouton 2 */
    fil[0] = 0;
    tel_bouton(&T, 2, TEL_BOUTON_LONG);
    tel_poll(&T, t0 + 60);
    carte_joue(cable[1], js, ligne);
    CHECK(strstr(fil, "b 2 long\n") != NULL, "le geste long part (%s)", fil);
    tel_poll(&T, t0 + 70);
    carte_joue(cable[1], js, ligne);
    tel_poll(&T, t0 + 80);
    CHECK(T.attente == TEL_ATT_RIEN, "le geste est confirme");
    CHECK(strcmp(tel_geste_nom(TEL_BOUTON_R3), "r3") == 0, "les gestes ont leurs noms");

    /* 5. une carte qui se tait : on le dit, on n'invente rien */
    tel_page(&T, 9);
    tel_poll(&T, t0 + 3000);
    tel_poll(&T, t0 + 5000);
    CHECK(T.muet, "une commande sans reponse rend la carte muette");
    CHECK(strstr(T.status, "ne repond pas") != NULL, "le statut le dit (%s)", T.status);

    /* 6. la fenetre du Mac affiche la dalle de la carte */
    G.wm.n = 0;
    int cable2[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, cable2) == 0, "un deuxieme cable");
    tel_attacher(&G_TEL, cable2[0]);
    app_open(&G, APP_TEL);
    win_t *w = wm_top_visible(&G.wm);
    CHECK(w && w->kind == APP_TEL, "la fenetre BATMino s'ouvre");
    app_render(&G, &ecran);
    CHECK(fb_ink(&ecran, w->x + 4, w->y + 4, w->w - 8, w->h - 8) > 300,
          "la fenetre est dessinee");
    fil[0] = 0;
    tel_poll(&G_TEL, 7000);
    carte_joue(cable2[1], js, ligne);
    CHECK(strstr(fil, "s\n") != NULL, "la fenetre demande la dalle a l'ouverture");
    tel_poll(&G_TEL, 7010);
    CHECK(G_TEL.ecran_lu, "la fenetre a recu la dalle");
    carte_joue(cable2[1], js, ligne);
    tel_poll(&G_TEL, 7020);
    CHECK(G_TEL.etat_lu, "la fenetre a recu l'etat");
    app_render(&G, &ecran);
    /* ce que la carte allume (100,50) doit s'allumer dans la fenetre */
    int px = w->x + 1 + (w->w - 2 - TEL_W - 2) / 2 + 1 + 100;
    int py = w->y + TITLE_H + 1 + 5 + 1 + 50;
    CHECK(fb_pget(&ecran, px, py) == 1, "le pixel (100,50) de la carte est a l'ecran");
    CHECK(fb_pget(&ecran, px + 2, py) == 0, "le pixel (102,50) reste blanc");

    /* 7. clic sur « page > » : le Mac pilote la carte */
    int bby = TITLE_H + 1 + (w->h - TITLE_H - 2) - 24;
    fil[0] = 0;
    int64_t gt = 7100;
    app_click(&G, w, 5 + 60 + 28, bby + 8, 0);          /* deuxieme touche */
    for (int i = 0; i < 3; i++) { tel_poll(&G_TEL, gt += 10); carte_joue(cable2[1], js, ligne); }
    CHECK(strstr(fil, "p 4\n") != NULL, "page > demande la page suivante (%s)", fil);

    /* 8. clic sur « bouton 1 » : un geste court */
    fil[0] = 0;
    app_click(&G, w, 5 + 2 * 60 + 28, bby + 8, 0);
    for (int i = 0; i < 4; i++) { tel_poll(&G_TEL, gt += 10); carte_joue(cable2[1], js, ligne); }
    CHECK(strstr(fil, "b 1 court\n") != NULL, "bouton 1 joue un geste court (%s)", fil);

    /* 9. le clavier : la fleche droite avance d'une page */
    fil[0] = 0;
    app_key_screen(&G, 0xff53, NULL);
    for (int i = 0; i < 4; i++) { tel_poll(&G_TEL, gt += 10); carte_joue(cable2[1], js, ligne); }
    CHECK(strstr(fil, "p 5\n") != NULL, "la fleche droite avance (%s)", fil);

    wm_close(&G.wm, w->id);
    tel_fermer(&G_TEL);
    tel_fermer(&T);
    close(cable[1]);
    close(cable2[1]);
    CHECK(1, "telecommande verifiee");
}

int main(void)
{
    /* un HOME de test : on ne touche pas au vrai disque */
    char tmp[] = "/tmp/oldmac-tests-XXXXXX";
    char *dir = mkdtemp(tmp);
    if (!dir) { perror("mkdtemp"); return 1; }
    setenv("HOME", dir, 1);

    fb_init(&ecran, SCR_W, SCR_H);
    t = 1000;
    sys_init(&G);
    G.rapide = 1;
    G.boot_phase = 2;

    test_fb();
    test_texte();
    test_disque();
    test_fenetres();
    test_demarrage();
    test_bureau();
    test_finder();
    test_macwrite();
    test_macpaint();
    test_menus();
    test_panneaux();
    test_clavier_machine();
    test_presse_papiers();
    test_persistance();
    test_menu_dessine();
    test_parcours();
    test_liaison();
    test_robustesse();

    printf("\n%d verifications, %d echec(s)\n", checks, fails);
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    if (system(cmd) != 0) { }
    return fails ? 1 : 0;
}
