
APPNAME=app

OBJS= \
	app.o \
	draw.o \
	main.o

C_COMPILER=clang
CXX_COMPILER=clang++
LINKER=clang++

all: app

.PHONY: app clean distclean all

#---
# these files are generated from XML descriptions of the Wayland protocol
#---

xdg-shell-protocol.c:
	wayland-scanner private-code \
	  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
	  > xdg-shell-protocol.c

xdg-shell-client-protocol.h:
	wayland-scanner client-header \
		< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
		> xdg-shell-client-protocol.h

#---
# normal Makefile stuff
#---

%.o: %.c
	${C_COMPILER} -c $*.c -o $*.o

%.o: %.cpp
	${CXX_COMPILER} -c $*.cpp -o $*.o

clean:
	rm *.o

distclean: clean
	rm xdg-shell-protocol.c
	rm xdg-shell-client-protocol.h

#---
# the app
#---

app: xdg-shell-client-protocol.h $(OBJS) xdg-shell-protocol.o
	${LINKER} $(OBJS) xdg-shell-protocol.o -o ${APPNAME} -lwayland-client -lrt
