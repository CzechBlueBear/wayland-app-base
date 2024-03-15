
APPNAME=app

OBJS= app.o \
	debug.o \
	draw.o \
	main.o

WAYLAND_OBJS= \
	xdg-shell-protocol.o \
	zxdg-decoration-protocol.o

WAYLAND_HEADERS= \
	xdg-shell-client-protocol.h \
	zxdg-decoration-client-protocol.h

LINK_LIBS=-lwayland-client -lrt

C_COMPILER=clang
CXX_COMPILER=clang++
LINKER=clang++
C_FLAGS=-ggdb -O
CXX_FLAGS=-ggdb -O

all: app

.PHONY: app clean distclean all

#---
# these files are generated from XML descriptions of the Wayland protocol
#---

xdg-shell-protocol.c:
	wayland-scanner private-code > $@ \
	  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

xdg-shell-client-protocol.h:
	wayland-scanner client-header > $@ \
		< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

zxdg-decoration-protocol.c:
	wayland-scanner private-code > $@ \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

zxdg-decoration-client-protocol.h:
	wayland-scanner client-header > $@ \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

#---
# normal Makefile stuff
#---

%.o: %.c
	${C_COMPILER} ${C_FLAGS} -c $*.c -o $*.o

%.o: %.cpp
	${CXX_COMPILER} ${CXX_FLAGS} -c $*.cpp -o $*.o

clean:
	rm -f *.o

distclean: clean
	rm -f xdg-shell-protocol.c
	rm -f xdg-shell-client-protocol.h
	rm -f zxdg-decoration-protocol.c
	rm -f zxdg-decoration-client-protocol.h

#---
# the app
#---

app: ${WAYLAND_HEADERS} ${OBJS} ${WAYLAND_OBJS}
	${LINKER} $(OBJS) $(WAYLAND_OBJS) -o ${APPNAME} ${LINK_LIBS}

