# oldmac

Un Macintosh compact de 1986, simule entierement : boitier beige, dalle
9 pouces en 1 bit, clavier, souris — et un Systeme qui demarre vraiment,
avec son Finder, ses fenetres, ses menus et son disque.

Rien n'est une image : le boitier, le clavier, la souris et chaque pixel de
l'ecran sont dessines par le programme (GTK4 + cairo). Rien n'est emprunte a
Apple : ni ROM, ni System, ni police. Tout ce que fait la machine est ecrit
ici, en C.

    make            # construit ./oldmac
    make tests      # 148 verifications, sans afficheur
    make install    # ~/.local/bin/oldmac + entree dans l'annuaire

## Utiliser la machine

Le programme ouvre une fenetre : le Macintosh au-dessus, son clavier et sa
souris en dessous.

- **souris reelle** : bouge le pointeur dans l'ecran ; clic = clic du Mac.
- **clavier reel** : tape, ca s'ecrit dans MacWrite.
- **clavier dessine** : chaque touche se clique et envoie son caractere.
- **souris dessinee** : cliquer sur son bouton clique a la position du
  pointeur (pratique quand on n'a que la souris).
- **barre de menus** : clic sur la pomme, Fichier, Edition, Present., Special.
- **fenetres** : deplacables par la barre de titre, fermables par la case de
  gauche, la derniere cliquee passe devant.

Au demarrage : la disquette au point d'interrogation, le Happy Mac, la barre
de progression du Systeme, puis le bureau. Un clic passe le demarrage.

## Ce que le Systeme sait faire

| Element | Ce qu'il fait |
|---|---|
| Finder | liste le disque, ouvre un document d'un double clic |
| MacWrite | frappe au clavier, curseur clignotant, enregistre en `.txt` |
| MacPaint | crayon, gomme, ligne, cadre, pot ; enregistre en `.pbm` |
| Tableau de bord | luminosite du tube, volume, et l'etat **reel** de la machine (hote, noyau, memoire, charge, duree) |
| A propos | Systeme 6.0.8, 1 Mo, ROM 128 Ko, et la signature du projet |
| Menus | Apple, Fichier, Edition (copier/coller), Present., Special (corbeille, extinction) |
| **BATMino** | la telecommande : la dalle 200 x 200 de la carte, en direct, et ses deux boutons |
| Alerte | poses de dialogue avec triangle d'attention et boutons |
| Horloge | l'heure vraie, dans la barre de menus ; se cache par le menu Apple |

Le disque n'est pas une maquette : les documents vivent dans

    ~/.local/share/oldmac/disk

On peut les ouvrir avec un editeur de texte ou un lecteur PBM de la machine.

## Piloter la carte BATMino

Le Mac a une carte e-paper posee a cote de lui (projet `batmino`, separe) :
double clic sur l'icone **BATMino** du bureau, ou menu Special > BATMino.

La fenetre montre la dalle 200 x 200 de la carte pixel pour pixel, et sous
elle ce que la carte raconte : le port, la page, le nombre d'elements, le
profil, le wifi, l'heure, la version du firmware. Quatre touches :

| Touche | Effet |
|---|---|
| `< page` / `page >` | page precedente / suivante |
| `bouton 1` / `bouton 2` | un geste court, comme un doigt sur la carte |
| clavier | fleches = pages, `b` et `n` = boutons, `r` = redemander l'image, `e` = redemander l'etat |

C'est le vrai protocole de la carte qui est parle, sur son port USB a
115200 bauds :

    e            -> une ligne JSON (page, curseur, mode, items, profil, wifi...)
    s            -> "FB " + 5000 octets en hexa (la dalle 200 x 200)
    p 7          -> "page 7"
    b 1 court    -> "bouton 1 court"      (court, double, long, r3)

Une seule commande en vol a la fois : la carte est petite. Sans reponse en
1,5 s elle est declaree muette et la fenetre le dit — elle n'affiche jamais
un etat invente : ce qui n'a pas ete lu s'ecrit « n.d. ».

    ./oldmac --port /dev/ttyACM0              # le port du cable
    python3 tools/faux_carte.py               # une carte de test sur un pty
    ./oldmac --port /dev/pts/2 --scene 7 --shot fenetre.png

## Fond d'ecran

    ./oldmac --fond mac.png --taille 2560x1440

L'ecran du Mac, sans boitier, sans pointeur, en noir et blanc pur, agrandi
au pixel le plus proche ; la trame du bureau continue jusqu'aux bords.

## Hors ecran (captures et mesures)

    ./oldmac --shot capture.png --scene 2   # capture PNG de la machine
    ./oldmac --info                          # etat du systeme simule
    ./oldmac --bench                         # vitesse de rendu

Scenes : 0 bureau, 1 Finder, 2 MacWrite, 3 MacPaint (le dessin de la photo),
4 tableau de bord, 5 a propos, 6 trois fenetres, 7 telecommande BATMino.

Sans `--scene`, la capture montre le demarrage ; `--t 2` avance le temps
simule de deux secondes.

## Architecture

| Fichier | Role |
|---|---|
| `oldmac.h` | les types : framebuffer 1 bit, disque, fenetres, systeme |
| `fb.c` | pixels, trames 25/50/75 %, lignes, texte rendu en 1 bit par Pango |
| `disk.c` | le disque virtuel (fichiers `.txt` et `.pbm`, corbeille) |
| `wm.c` | empilement des fenetres, menus et leurs etats |
| `apps.c` | le Systeme : bureau, barre, Finder, MacWrite, MacPaint, dialogues |
| `telecommande.c` | la liaison avec la carte BATMino : port serie, protocole, etats |
| `ui.c` | la machine : boitier, lecteur, clavier, souris, dalle et scanlines |
| `main.c` | fenetre GTK4, vraies entrees, modes hors ecran |
| `tests.c` | les 148 verifications |
| `tools/faux_carte.py` | une carte de test sur un pty, qui affiche une mire |

Deux disciplines tenues partout :

1. **l'ecran est en 1 bit** — 512 x 342, une valeur par pixel, mise a
   l'echelle au dessin ; aucun flou, aucun anti-aliasing sur la dalle ;
2. **une valeur indisponible s'affiche « n.d. »** — on n'invente pas un nom
   d'hote ni une memoire : ce qui est affiche est lu sur la machine.

## Limites assumées

- ce n'est pas un emulateur 68000 : il n'y a pas de ROM Apple, donc pas de
  System d'origine. Le Systeme 6.0.8 affiche est une reconstitution ecrite
  pour ce projet ;
- la police n'est pas Chicago (introuvable en libre) mais DejaVu Sans rendue
  en 1 bit, ce qui garde l'aspect tramé d'epoque ;
- pas de son : le haut-parleur est dessine et son curseur bouge, mais aucun
  echantillon n'est joue.

## Projet separe

`oldmac` est un projet de bureau, independant des firmwares ESP32 (batmino,
bleu, pocket-deepseek) : aucun fichier, aucune bibliotheque, aucun depot
partage avec eux.

oldmac 1.1.0 — @paristzou — juste pour les filles.
