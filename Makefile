INSTALLDIR=/usr/local
TARGETS=msg contacts

REQ_PKGS += telepathy-glib gio-2.0
LDLIBS:=$(shell pkg-config --libs $(REQ_PKGS) x11)
PKG_CFLAGS:=$(shell pkg-config --cflags $(REQ_PKGS))
CFLAGS += -std=c99 $(PKG_CFLAGS) -ggdb -W -Wall -Wextra -pedantic

SRC = $(wildcard src/*.c)
HEAD = $(wildcard src/*.h)
OBJ  = $(foreach obj, $(SRC:.c=.o),  $(notdir $(obj)))

.PHONY: clean install

all: ${TARGETS}

VPATH:=src
${OBJ}: ${HEAD}

msg: msg.o shared.o

contacts: contacts.o shared.o

clean:
	rm -f ${OBJ}
	rm ${TARGETS}

install: ${TARGETS}
	install -m755 msg $(INSTALLDIR)/bin/msg
	install -m755 contacts $(INSTALLDIR)/bin/contacts
