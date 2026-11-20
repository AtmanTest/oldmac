#!/usr/bin/env python3
"""faux_carte.py — une carte BATMino de test, sur un pty, qui parle le
vrai protocole de la carte : e (etat JSON), s (dalle en hexa), p N, b N geste.

Elle ne joue rien et n'invente aucun chiffre de jeu : sa dalle est une mire
geométrique et son profil s'appelle MIRE. Elle sert a verifier la
telecommande d'oldmac sans carte sous la main.

    python3 tools/faux_carte.py            # affiche le nom du pty
    oldmac --port /dev/pts/N --scene 7 --shot fenetre.png
"""
import json
import os
import pty
import select
import signal
import sys

W = H = 200


def mire():
    px = [[0] * W for _ in range(H)]
    # cadre de 3 pixels
    for y in range(H):
        for x in range(W):
            if x < 3 or y < 3 or x >= W - 3 or y >= H - 3:
                px[y][x] = 1
    # damier 8x8 : le morceau qui montre le contraste
    for y in range(14, 74):
        for x in range(14, 74):
            if ((x // 8) + (y // 8)) % 2 == 0:
                px[y][x] = 1
    # disque plein
    cx, cy, r = 140, 96, 44
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if 0 <= x < W and 0 <= y < H and (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                px[y][x] = 1
    # deux diagonales
    for i in range(0, 190):
        for (x, y) in ((14 + i, 150 + i // 3), (190 - i, 150 + i // 3)):
            if 0 <= x < W and 0 <= y < H:
                px[y][x] = 1
    # bande de traits verticaux, de plus en plus fins
    x = 14
    largeur = 6
    while x < W - 14 and largeur >= 1:
        for y in range(186 - 24, 186):
            for k in range(largeur):
                if x + k < W - 4:
                    px[y][x + k] = 1
        x += largeur * 2
        largeur -= 1
    return px


def trame_hexa(px):
    octets = bytearray()
    for y in range(H):
        for xb in range(0, W, 8):
            v = 0
            for b in range(8):
                if px[y][xb + b]:
                    v |= 0x80 >> b
            octets.append(v)
    return "FB " + octets.hex()


ETAT = {
    "page": 7, "curseur": 1, "mode": 0, "items": 5,
    "profil": "MIRE", "solde_ct": 0, "hist": 0, "top": 0,
    "wifi": False, "heure": True,
    "rendu_ms": 0, "ecran_ms": 0,
    "heap": 0, "psram": 0, "version": "mire",
}


def main():
    maitre, esclave = pty.openpty()
    nom = os.ttyname(esclave)
    print(nom, flush=True)
    ligne = trame_hexa(mire())
    tampon = b""
    signal.signal(signal.SIGTERM, lambda *a: sys.exit(0))
    while True:
        try:
            pret, _, _ = select.select([maitre], [], [], 1.0)
        except (OSError, KeyboardInterrupt):
            break
        if not pret:
            continue
        try:
            d = os.read(maitre, 4096)
        except OSError:
            break
        if not d:
            break
        tampon += d
        while b"\n" in tampon:
            l, tampon = tampon.split(b"\n", 1)
            t = l.decode("utf-8", "replace").strip()
            if not t:
                continue
            if t[0] == "e":
                rep = json.dumps(ETAT, separators=(",", ":")) + "\n"
            elif t[0] == "s":
                rep = ligne + "\n"
            elif t.startswith("p "):
                rep = "page %d\n" % int(t[2:] or 1)
            elif t.startswith("b "):
                rep = "bouton %s\n" % t[2:]
            else:
                rep = "?\n"
            os.write(maitre, rep.encode())


if __name__ == "__main__":
    main()
