CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
PKGS     = gtk4 cairo pangocairo
SRC      = main.c fb.c disk.c wm.c apps.c ui.c telecommande.c
OBJ      = $(SRC:.c=.o)

all: oldmac

oldmac: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $$(pkg-config --cflags --libs $(PKGS)) -lm

%.o: %.c oldmac.h
	$(CC) $(CFLAGS) -c -o $@ $< $$(pkg-config --cflags $(PKGS))

tests: tests_oldmac
	./tests_oldmac

tests_oldmac: tests.c fb.c disk.c wm.c apps.c ui.c telecommande.c
	$(CC) $(CFLAGS) -o $@ tests.c fb.c disk.c wm.c apps.c ui.c telecommande.c \
	  $$(pkg-config --cflags --libs $(PKGS)) -lm

install: oldmac
	install -Dm755 oldmac $(HOME)/.local/bin/oldmac
	install -Dm644 org.taz.oldmac.desktop $(HOME)/.local/share/applications/org.taz.oldmac.desktop
	install -Dm644 icon/org.taz.oldmac.svg $(HOME)/.local/share/icons/hicolor/scalable/apps/org.taz.oldmac.svg
	@command -v update-desktop-database >/dev/null && update-desktop-database $(HOME)/.local/share/applications || true

clean:
	rm -f oldmac tests_oldmac $(OBJ)

.PHONY: all install clean tests
