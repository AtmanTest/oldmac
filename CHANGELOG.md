# Journal des versions

## 1.1.0 — 2026-11-20 — La telecommande BATMino, et un fond d'ecran

Le Mac ne simule plus seulement : il pilote la carte e-paper posee a cote.

### La telecommande (`telecommande.c`)
- Une icone « BATMino » sur le bureau (et une entree dans le menu Special) :
  un double clic ouvre une fenetre qui montre la dalle 200 x 200 de la carte,
  pixel pour pixel, telle qu'elle est.
- Le vrai protocole de la carte, sur son port serie a 115200 bauds : `e`
  (etat JSON), `s` (dalle en hexa), `p N` (page), `b N court` (geste).
- Quatre touches dessinees : `< page`, `page >`, `bouton 1`, `bouton 2` ;
  clavier : fleches pour les pages, `b` et `n` pour les boutons, `r` et `e`
  pour redemander l'image ou l'etat.
- Une seule commande en vol a la fois ; sans reponse en 1,5 s la carte est
  declaree muette. Ce qui n'est pas lu s'affiche « n.d. », jamais zero.
- `--port PERIPH` choisit le port (defaut `/dev/ttyACM0`).

### Le fond d'ecran
- `--fond FICHIER` ecrit l'ecran du Mac en noir et blanc pur, sans boitier,
  sans pointeur, agrandi au pixel le plus proche ; la trame du bureau
  continue jusqu'aux bords. `--taille LxH` (defaut 2560x1440).

### Le contraste
- Phosphore plus clair, encre plus noire, lignes de balayage plus discretes,
  luminosite par defaut a 92 % : la dalle se detache mieux sur une photo.

### Les outils
- `tools/faux_carte.py` : une carte de test sur un pty qui parle le vrai
  protocole et affiche une mire — de quoi verifier la liaison sans carte.
- `make tests` : 148 verifications, dont la liaison complete sur un faux
  cable (etat, trame, page, geste, silence, clics et clavier de la fenetre).

## 1.0.0 — 2026-11-20 — Macintosh compact simule

Premiere version complete : une machine entiere, dessinee et simulee, dans
une fenetre GTK4.

### La machine
- Boitier beige dessine au cairo : chanfreins, ombre portee, creux d'ecran,
  lecteur de disquette, badge pomme arc-en-ciel, bouton de luminosite, diode.
- Dalle 512 x 342 en 1 bit avec lignes de balayage, reflet de verre et
  luminosite pilotee par le tableau de bord.
- Clavier et souris dessines, cliquables : chaque touche envoie son caractere,
  le bouton de la souris clique a la position du pointeur.

### Le Systeme
- Demarrage en trois temps : disquette inconnue, Happy Mac, barre de
  progression du Systeme 6.0.8, puis bureau.
- Bureau en trame grise, sept icones (disque, MacWrite, MacPaint, corbeille),
  selection au clic, ouverture au double clic.
- Gestionnaire de fenetres : empilement, fenetre active (barre a rayures),
  deplacement par la barre de titre, case de fermeture, case de zoom.
- Cinq menus deroulants avec separateurs, entrees inactives, coche d'etat et
  survol ; barre de menus avec pomme et horloge a l'heure vraie.
- Finder : liste du disque, entete (nombre d'elements, Ko libres), selection.
- MacWrite : frappe, retour arriere, curseur clignotant, repli des lignes,
  pied de page (nom, nombre de caracteres, modifie), copier/coller.
- MacPaint : cinq outils (crayon, gomme, ligne, cadre, pot), deux trames,
  toile 320 x 200, enregistrement PBM.
- Tableau de bord : luminosite, volume, et l'etat **reel** de la machine
  (hote, noyau, memoire, charge, duree) a cote du Mac simule.
- A propos, alertes avec triangle d'attention, eteindre et rallumer.

### Le disque
- Disque virtuel de 800 Ko dans `~/.local/share/oldmac/disk` : documents
  `.txt` et dessins `.pbm` reels, corbeille, vidage, espace occupe calcule.
- Les documents survivent a la fermeture du programme.

### Outils
- `make tests` : 96 verifications sans afficheur (framebuffer, texte,
  disque, fenetres, demarrage, bureau, Finder, MacWrite, MacPaint, menus,
  panneaux, clavier de la machine, presse-papiers, persistance, robustesse).
- `--shot` (capture PNG), `--scene` (0 a 6), `--t` (temps simule),
  `--info`, `--bench`.

### Notes
- Aucune ROM, aucun System, aucune police d'Apple : tout est ecrit ici.
- Compile sans un seul avertissement avec `-Wall -Wextra`.
