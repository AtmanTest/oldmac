/*  telecommande.c — la liaison avec la carte BATMino, depuis le Mac.
 *
 *  Rien que du C : pas de GTK ici, pour que les tests puissent brancher
 *  un faux cable (socketpair ou pty) et parler le vrai protocole.
 *
 *  La carte repond, sur son port USB a 115200 bauds :
 *      e            -> une ligne JSON : page, curseur, mode, items, profil...
 *      s            -> "FB " suivi de 5000 octets en hexa (la dalle 200x200)
 *      p N          -> "page N"
 *      b N court    -> "bouton N court"      (court, double, long, r3)
 *
 *  Une seule commande en vol a la fois : la carte est petite, elle a une
 *  seule bouche. Ce qui ne repond pas en 1,5 s est declare muet, et on le
 *  dit plutot que d'inventer un etat.
 */
#include "oldmac.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

tel_t G_TEL;

static const char *GESTES[TEL_GESTES] = { "court", "double", "long", "r3" };

static int64_t maintenant(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

const char *tel_geste_nom(int g)
{
    return (g >= 0 && g < TEL_GESTES) ? GESTES[g] : "court";
}

void tel_init(tel_t *t, const char *port)
{
    memset(t, 0, sizeof(*t));
    t->fd = -1;
    t->dem_p = -1;
    t->dem_btn = 0;
    snprintf(t->port, sizeof(t->port), "%s",
             (port && *port) ? port : TEL_PORT_DEFAUT);
    snprintf(t->status, sizeof(t->status), "carte non branchee");
    t->dernier_essai_ms = maintenant();
    t->dernier_etat_ms = t->dernier_essai_ms;
}

/* ------------------------------------------------------------------ */
/* le port                                                            */
/* ------------------------------------------------------------------ */
void tel_fermer(tel_t *t)
{
    if (t->fd >= 0) close(t->fd);
    t->fd = -1;
    t->ouvert = 0;
    t->joint = 0;
    t->attente = 0;
    t->rx_n = 0;
    snprintf(t->status, sizeof(t->status), "carte non branchee");
}

int tel_ouvrir(tel_t *t)
{
    if (t->fd >= 0) return 1;
    int fd = open(t->port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        snprintf(t->status, sizeof(t->status), "pas de cable (%.40s)", t->port);
        t->dernier_essai_ms = maintenant();
        return 0;
    }
    struct termios a;
    if (tcgetattr(fd, &a) == 0) {
        cfmakeraw(&a);
        cfsetispeed(&a, B115200);
        cfsetospeed(&a, B115200);
        a.c_cflag |= (CLOCAL | CREAD);
        a.c_cc[VMIN] = 0;
        a.c_cc[VTIME] = 0;
        tcsetattr(fd, TCSANOW, &a);
    }
    tcflush(fd, TCIOFLUSH);
    t->fd = fd;
    t->ouvert = 1;
    t->joint = 0;
    t->rx_n = 0;
    t->attente = 0;
    t->dernier_essai_ms = maintenant();
    t->dernier_etat_ms = 0;              /* on demande l'etat tout de suite */
    snprintf(t->status, sizeof(t->status), "la carte ne repond pas");
    return 1;
}

/* tests : un faux brut deja pret (socketpair, pty) */
void tel_attacher(tel_t *t, int fd)
{
    tel_fermer(t);
    /* un vrai port serie est ouvert en non bloquant : on fait pareil */
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    t->fd = fd;
    t->ouvert = 1;
    t->joint = 0;
    t->rx_n = 0;
    t->attente = 0;
    t->dernier_essai_ms = maintenant();
    t->dernier_etat_ms = t->dernier_essai_ms;
    snprintf(t->status, sizeof(t->status), "la carte ne repond pas");
}

static void envoyer(tel_t *t, const char *cmd, int type, int64_t now)
{
    size_t n = strlen(cmd), fait = 0;
    while (fait < n) {
        ssize_t k = write(t->fd, cmd + fait, n - fait);
        if (k > 0) { fait += (size_t)k; continue; }
        if (k < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        break;                            /* le cable a lache */
    }
    t->attente = type;
    t->attente_ms = now;
    t->envois++;
}

/* ------------------------------------------------------------------ */
/* ce que la carte dit                                                */
/* ------------------------------------------------------------------ */
static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int champ_int(const char *j, const char *cle, int def)
{
    char motif[32];
    snprintf(motif, sizeof(motif), "\"%s\":", cle);
    const char *p = strstr(j, motif);
    if (!p) return def;
    p += strlen(motif);
    while (*p == ' ') p++;
    if (*p < '0' || *p > '9') return def;
    return atoi(p);
}

static void champ_txt(const char *j, const char *cle, char *out, size_t cap,
                      const char *def)
{
    char motif[32];
    snprintf(motif, sizeof(motif), "\"%s\":\"", cle);
    const char *p = strstr(j, motif);
    snprintf(out, cap, "%s", def);
    if (!p) return;
    p += strlen(motif);
    const char *f = strchr(p, '"');
    if (!f) return;
    size_t n = (size_t)(f - p);
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = 0;
}

static int champ_bool(const char *j, const char *cle)
{
    char motif[32];
    snprintf(motif, sizeof(motif), "\"%s\":", cle);
    const char *p = strstr(j, motif);
    if (!p) return -1;
    p += strlen(motif);
    if (!strncmp(p, "true", 4)) return 1;
    if (!strncmp(p, "false", 5)) return 0;
    return -1;
}

static void lire_json(tel_t *t, const char *j)
{
    t->page    = champ_int(j, "page", t->page);
    t->curseur = champ_int(j, "curseur", t->curseur);
    t->mode    = champ_int(j, "mode", t->mode);
    t->items   = champ_int(j, "items", t->items);
    t->solde_ct = champ_int(j, "solde_ct", t->solde_ct);
    t->hist    = champ_int(j, "hist", t->hist);
    t->top     = champ_int(j, "top", t->top);
    t->wifi    = champ_bool(j, "wifi");
    t->heure   = champ_bool(j, "heure");
    champ_txt(j, "profil", t->profil, sizeof(t->profil), "n.d.");
    champ_txt(j, "version", t->version, sizeof(t->version), "n.d.");
    t->etat_lu = 1;
}

static void lire_trame(tel_t *t, const char *h)
{
    size_t n = strlen(h);
    if (n < (size_t)TEL_FB_OCTETS * 2) return;      /* trame tronquee */
    for (int i = 0; i < TEL_FB_OCTETS; i++) {
        int a = hexval((unsigned char)h[i * 2]);
        int b = hexval((unsigned char)h[i * 2 + 1]);
        if (a < 0 || b < 0) return;
        t->ecran[i] = (uint8_t)((a << 4) | b);
    }
    t->ecran_lu = 1;
}

static void traiter_ligne(tel_t *t, int64_t now)
{
    t->rx[t->rx_n] = 0;
    const char *l = t->rx;

    if (!strncmp(l, "FB ", 3)) {
        lire_trame(t, l + 3);
    } else if (l[0] == '{') {
        lire_json(t, l);
    } else if (!strncmp(l, "page ", 5)) {
        int p = atoi(l + 5);
        if (p > 0) t->page = p;
        t->dem_ecran = 1;                       /* on veut voir la suite */
    } else if (!strncmp(l, "bouton ", 7)) {
        t->dem_ecran = 1;
    } else if (!strncmp(l, "aucune image", 12)) {
        t->ecran_lu = 0;
    }
    t->attente = 0;
    t->joint = 1;
    t->muet = 0;
    t->dernier_rx_ms = now;
    t->lignes++;
    snprintf(t->status, sizeof(t->status), "carte prete");
}

static void recevoir(tel_t *t, const char *d, size_t n, int64_t now)
{
    for (size_t i = 0; i < n; i++) {
        char c = d[i];
        if (c == '\r') continue;
        if (c == '\n') {
            if (t->rx_n) traiter_ligne(t, now);
            t->rx_n = 0;
            continue;
        }
        if (t->rx_n + 1 < sizeof(t->rx)) t->rx[t->rx_n++] = c;
        else t->rx_n = 0;                  /* ligne absurde : on la jette */
    }
}

/* ------------------------------------------------------------------ */
/* ce qu'on demande                                                   */
/* ------------------------------------------------------------------ */
void tel_demander_etat(tel_t *t)  { t->dernier_etat_ms = 0; }
void tel_demander_ecran(tel_t *t) { t->dem_ecran = 1; }

void tel_page(tel_t *t, int n)
{
    if (n < 1) n = 1;
    t->dem_p = n;
}

void tel_bouton(tel_t *t, int n, int geste)
{
    if (n != 1 && n != 2) return;
    if (geste < 0 || geste >= TEL_GESTES) geste = TEL_BOUTON_COURT;
    t->dem_btn = n;
    t->dem_geste = geste;
}

int tel_page_suivante(const tel_t *t, int pas)
{
    int p = t->page > 0 ? t->page + pas : 0;
    if (p < 1) p = 1;
    return p;
}

/* ------------------------------------------------------------------ */
/* une passe de la liaison                                            */
/* ------------------------------------------------------------------ */
int tel_poll(tel_t *t, int64_t now)
{
    int change = 0;

    if (!t->ouvert || t->fd < 0) {
        if (now - t->dernier_essai_ms >= 2000) {
            t->dernier_essai_ms = now;
            tel_ouvrir(t);
            change = 1;
        }
        return change;
    }

    /* tout ce qui est arrive */
    char buf[4096];
    for (;;) {
        ssize_t k = read(t->fd, buf, sizeof(buf));
        if (k > 0) { recevoir(t, buf, (size_t)k, now); change = 1; continue; }
        if (k == 0) break;
        if (errno == EINTR) continue;
        break;                                   /* EAGAIN : plus rien */
    }

    /* une commande restee sans reponse */
    if (t->attente && now - t->attente_ms > 1500) {
        t->attente = 0;
        t->muet = 1;
        snprintf(t->status, sizeof(t->status), "la carte ne repond pas");
        change = 1;
    }

    /* une seule commande en vol */
    if (!t->attente) {
        if (t->dem_p > 0) {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "p %d\n", t->dem_p);
            t->dem_p = -1;
            envoyer(t, cmd, TEL_ATT_PAGE, now);
            change = 1;
        } else if (t->dem_btn) {
            char cmd[48];
            snprintf(cmd, sizeof(cmd), "b %d %s\n", t->dem_btn,
                     tel_geste_nom(t->dem_geste));
            t->dem_btn = 0;
            envoyer(t, cmd, TEL_ATT_GESTE, now);
            change = 1;
        } else if (t->dem_ecran) {
            t->dem_ecran = 0;
            envoyer(t, "s\n", TEL_ATT_ECRAN, now);
            change = 1;
        } else if (now - t->dernier_etat_ms > 2000) {
            t->dernier_etat_ms = now;
            envoyer(t, "e\n", TEL_ATT_ETAT, now);
            change = 1;
        }
    }
    return change;
}
