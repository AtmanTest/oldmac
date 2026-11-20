/*  wm.c — gestionnaire de fenetres du Systeme simule (empilement,
 *  deplacement, fermeture) et definition des menus de la barre.
 */
#include "oldmac.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void wm_init(wm_t *m)
{
    memset(m, 0, sizeof(*m));
    m->next_id = 1;
    m->next_y = 40;
}

win_t *wm_open(wm_t *m, int kind, const char *title, int x, int y, int w, int h)
{
    if (m->n >= MAX_WIN) {
        /* on ferme la fenetre la plus basse pour faire de la place */
        int id = m->z[0];
        wm_close(m, id);
    }
    win_t *wi = &m->w[m->n];
    memset(wi, 0, sizeof(*wi));
    wi->id = m->next_id++;
    wi->kind = kind;
    wi->x = x; wi->y = y; wi->w = w; wi->h = h;
    snprintf(wi->title, sizeof(wi->title), "%s", title);
    wi->visible = 1;
    wi->active = 1;
    m->z[m->n] = wi->id;
    m->n++;
    wm_raise(m, wi->id);
    return wi;
}

win_t *wm_by_id(wm_t *m, int id)
{
    for (int i = 0; i < m->n; i++) if (m->w[i].id == id) return &m->w[i];
    return NULL;
}

void wm_close(wm_t *m, int id)
{
    win_t *w = wm_by_id(m, id);
    if (!w) return;
    app_close(w);
    int idx = (int)(w - m->w);
    for (int i = idx; i < m->n - 1; i++) m->w[i] = m->w[i + 1];
    m->n--;
    int zn = 0;
    for (int i = 0; i < m->n + 1 && i < MAX_WIN; i++)
        if (m->z[i] != id) m->z[zn++] = m->z[i];
    m->z[zn] = 0;
    if (zn > 0) {
        win_t *top = wm_by_id(m, m->z[zn - 1]);
        for (int i = 0; i < m->n; i++) m->w[i].active = 0;
        if (top) top->active = 1;
    }
}

void wm_raise(wm_t *m, int id)
{
    int pos = -1;
    for (int i = 0; i < m->n; i++) if (m->z[i] == id) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i < m->n - 1; i++) m->z[i] = m->z[i + 1];
    m->z[m->n - 1] = id;
    for (int i = 0; i < m->n; i++) m->w[i].active = (m->w[i].id == id);
}

win_t *wm_top_visible(wm_t *m)
{
    for (int i = m->n - 1; i >= 0; i--) {
        win_t *w = wm_by_id(m, m->z[i]);
        if (w && w->visible) return w;
    }
    return NULL;
}

win_t *wm_at(wm_t *m, int x, int y)
{
    for (int i = m->n - 1; i >= 0; i--) {
        win_t *w = wm_by_id(m, m->z[i]);
        if (!w || !w->visible) continue;
        if (x >= w->x && y >= w->y && x < w->x + w->w && y < w->y + w->h) return w;
    }
    return NULL;
}

void wm_lower_all(wm_t *m)
{
    for (int i = 0; i < m->n; i++) m->w[i].active = 0;
}

/* ------------------------------------------------------------------ */
/* menus                                                              */
/* ------------------------------------------------------------------ */
const char *MENU_TITRES[5] = { "Apple", "Fichier", "Edition", "Present.", "Special" };

static const char *MENUS[5][10] = {
    { "A propos de ce Mac", "Tableau de bord", "Horloge", "--",
      "MacWrite", "MacPaint", "Le Finder", "--", "Redemarrer", "Eteindre" },
    { "Nouveau", "Ouvrir...", "Fermer", "Enregistrer", "--",
      "Imprimer l'ecran", "--", "Mettre a la corbeille", NULL, NULL },
    { "Annuler", "--", "Couper", "Copier", "Coller", "Tout effacer", NULL, NULL, NULL, NULL },
    { "Trier par nom", "Trier par date", "--", "Infos du disque", "Rafraichir", NULL, NULL, NULL, NULL, NULL },
    { "Vider la corbeille", "--", "Plein ecran", "A propos du projet", NULL, NULL, NULL, NULL, NULL, NULL },
};

static const int MENUS_N[5] = { 10, 8, 6, 5, 4 };

int menu_nitems(int m) { return (m >= 0 && m < 5) ? MENUS_N[m] : 0; }

const char *menu_item(int m, int i)
{
    if (m < 0 || m >= 5 || i < 0 || i >= MENUS_N[m]) return NULL;
    return MENUS[m][i];
}

bool menu_item_checked(int m, int i)
{
    const char *t = menu_item(m, i);
    if (!t) return false;
    if (strcmp(t, "Horloge") == 0) return G.horloge != 0;
    return false;
}

bool menu_item_disabled(int m, int i)
{
    const char *t = menu_item(m, i);
    if (!t) return true;
    if (strcmp(t, "--") == 0) return true;
    win_t *top = wm_top_visible(&G.wm);
    if (m == 1) {
        if (strcmp(t, "Fermer") == 0 || strcmp(t, "Enregistrer") == 0 ||
            strcmp(t, "Imprimer l'ecran") == 0) return top == NULL;
        if (strcmp(t, "Mettre a la corbeille") == 0) return top == NULL;
    }
    if (m == 2 && top && top->kind == APP_FINDER &&
        (strcmp(t, "Couper") == 0 || strcmp(t, "Copier") == 0)) return true;
    return false;
}
