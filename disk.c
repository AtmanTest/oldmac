/*  disk.c — disque virtuel du Mac simule.
 *  Les documents vivent vraiment sur le disque de la machine, dans
 *  ~/.local/share/oldmac/disk : on peut les ouvrir avec un editeur normal.
 */
#include "oldmac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>

static void pourcent(char *dst, size_t n, const char *src)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < n; i++) {
        char c = src[i];
        if (c == '_') c = ' ';
        dst[j++] = c;
    }
    dst[j] = 0;
}

void disk_init(disk_t *d)
{
    memset(d, 0, sizeof(*d));
    const char *home = getenv("HOME");
    snprintf(d->root, sizeof(d->root), "%s/.local/share/oldmac/disk", home ? home : ".");
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/Corbeille'", d->root);
    if (system(cmd) != 0) { /* pas de shell : on cree a la main */ }
    mkdir(d->root, 0755);
    char tr[600];
    snprintf(tr, sizeof(tr), "%s/Corbeille", d->root);
    mkdir(tr, 0755);
    disk_rescan(d);
}

void disk_rescan(disk_t *d)
{
    d->n = 0;
    d->trash_n = 0;
    DIR *dp = opendir(d->root);
    if (!dp) return;
    struct dirent *e;
    while ((e = readdir(dp)) && d->n < DISK_MAX) {
        const char *n = e->d_name;
        if (n[0] == '.') continue;
        if (strcmp(n, "Corbeille") == 0) continue;
        size_t len = strlen(n);
        ftype_t t;
        if (len > 4 && strcmp(n + len - 4, ".txt") == 0) t = FT_TEXT;
        else if (len > 4 && (strcmp(n + len - 4, ".pbm") == 0)) t = FT_PAINT;
        else continue;
        dfile_t *f = &d->f[d->n];
        snprintf(f->path, sizeof(f->path), "%s/%s", d->root, n);
        snprintf(f->name, sizeof(f->name), "%.*s", (int)(len - 4), n);
        pourcent(f->name, sizeof(f->name), f->name);
        f->type = t;
        struct stat st;
        f->size = stat(f->path, &st) == 0 ? (int)st.st_size : 0;
        d->n++;
    }
    closedir(dp);

    char tr[600];
    snprintf(tr, sizeof(tr), "%s/Corbeille", d->root);
    DIR *dt = opendir(tr);
    if (dt) {
        while ((e = readdir(dt)) && d->trash_n < DISK_MAX) {
            if (e->d_name[0] == '.') continue;
            snprintf(d->trash_names[d->trash_n], NAME_MAX_LEN, "%.39s", e->d_name);
            d->trash_n++;
        }
        closedir(dt);
    }
}

static void nom_vers_fichier(char *dst, size_t n, const char *nom, const char *ext)
{
    size_t j = 0;
    for (size_t i = 0; nom[i] && j + 5 < n; i++) {
        char c = nom[i];
        if (c == ' ') c = '_';
        if (c == '/' || c == '\'' || c == '"' || c == ':') continue;
        dst[j++] = c;
    }
    dst[j] = 0;
    snprintf(dst + j, n - j, ".%s", ext);
}

int disk_write_text(disk_t *d, const char *name, const char *data)
{
    char fn[64];
    nom_vers_fichier(fn, sizeof(fn), name, "txt");
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", d->root, fn);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(data, 1, strlen(data), f);
    fclose(f);
    disk_rescan(d);
    return 1;
}

char *disk_read(disk_t *d, const char *name, int *len)
{
    for (int i = 0; i < d->n; i++) {
        if (strcmp(d->f[i].name, name) != 0) continue;
        FILE *f = fopen(d->f[i].path, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = calloc(1, (size_t)sz + 1);
        size_t rd = fread(buf, 1, (size_t)sz, f);
        buf[rd] = 0;
        fclose(f);
        if (len) *len = (int)rd;
        return buf;
    }
    return NULL;
}

int disk_write_paint(disk_t *d, const char *name, const uint8_t *bits, int w, int h)
{
    char fn[64];
    nom_vers_fichier(fn, sizeof(fn), name, "pbm");
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", d->root, fn);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P1\n%d %d\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) fputc(bits[(size_t)y * w + x] ? '1' : '0', f);
        fputc('\n', f);
    }
    fclose(f);
    disk_rescan(d);
    return 1;
}

uint8_t *disk_read_paint(disk_t *d, const char *name, int *w, int *h)
{
    for (int i = 0; i < d->n; i++) {
        if (strcmp(d->f[i].name, name) != 0) continue;
        FILE *f = fopen(d->f[i].path, "rb");
        if (!f) return NULL;
        char magic[3] = {0};
        int ww = 0, hh = 0;
        if (fscanf(f, "%2s %d %d", magic, &ww, &hh) != 3 || ww <= 0 || hh <= 0) {
            fclose(f);
            return NULL;
        }
        uint8_t *buf = calloc((size_t)ww * (size_t)hh, 1);
        for (int k = 0; k < ww * hh; ) {
            int c = fgetc(f);
            if (c == EOF) break;
            if (c == '1') buf[k++] = 1;
            else if (c == '0') buf[k++] = 0;
        }
        fclose(f);
        *w = ww; *h = hh;
        return buf;
    }
    return NULL;
}

void disk_trash(disk_t *d, const char *name)
{
    for (int i = 0; i < d->n; i++) {
        if (strcmp(d->f[i].name, name) != 0) continue;
        char dst[700];
        const char *base = strrchr(d->f[i].path, '/');
        snprintf(dst, sizeof(dst), "%s/Corbeille/%s", d->root, base ? base + 1 : name);
        if (rename(d->f[i].path, dst) != 0) unlink(d->f[i].path);
        disk_rescan(d);
        return;
    }
}

void disk_empty_trash(disk_t *d)
{
    char cmd[700];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/Corbeille'/*", d->root);
    if (system(cmd) != 0) { }
    disk_rescan(d);
}

int disk_capacity_ko(void) { return 800; }   /* un disque de 800 Ko, comme en 1986 */

int disk_used_ko(void)
{
    long total = 0;
    const char *home = getenv("HOME");
    char root[400];
    snprintf(root, sizeof(root), "%s/.local/share/oldmac/disk", home ? home : ".");
    DIR *dp = opendir(root);
    if (!dp) return 0;
    struct dirent *e;
    while ((e = readdir(dp))) {
        if (e->d_name[0] == '.') continue;
        char path[600];
        snprintf(path, sizeof(path), "%.300s/%.200s", root, e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) total += (long)st.st_size;
    }
    closedir(dp);
    return (int)((total + 1023) / 1024) + 1;   /* +1 : le systeme lui-meme */
}

int disk_free_ko(void) { return disk_capacity_ko() - disk_used_ko(); }
