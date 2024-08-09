
APPNAME=app

BUILDDIR=build
SRCDIR=src
GENSRCDIR=src/generated

C_COMPILER=clang
CXX_COMPILER=clang++
LINKER=clang++
C_FLAGS=-ggdb -O
CXX_FLAGS=-ggdb -O

#SWITCHES=-DUSE_EGL=1
SWITCHES=

OBJS= ${BUILDDIR}/app.o \
	${BUILDDIR}/debug.o \
	${BUILDDIR}/draw.o \
	${BUILDDIR}/main.o \
	${BUILDDIR}/frame.o

WAYLAND_OBJS= \
	${BUILDDIR}/xdg-shell-protocol.o \
	${BUILDDIR}/zxdg-decoration-protocol.o

WAYLAND_HEADERS= \
	${SRCDIR}/generated/xdg-shell-client-protocol.h \
	${SRCDIR}/generated/zxdg-decoration-client-protocol.h

INCLUDES=-I${SRCDIR} -I${SRCDIR}/generated

LINK_LIBS=-lwayland-client -lrt

all: app

.PHONY: app clean distclean all

#---
# these files are generated from XML descriptions of the Wayland protocol
#---

${GENSRCDIR}/xdg-shell-protocol.c:
	wayland-scanner private-code > $@ \
	  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

${GENSRCDIR}/xdg-shell-client-protocol.h:
	wayland-scanner client-header > $@ \
		< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

${GENSRCDIR}/zxdg-decoration-protocol.c:
	wayland-scanner private-code > $@ \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

${GENSRCDIR}/zxdg-decoration-client-protocol.h:
	wayland-scanner client-header > $@ \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

#---
# normal Makefile stuff
#---

${BUILDDIR}/%.o: ${GENSRCDIR}/%.c Makefile ${GENSRCDIR}/*.h
	${C_COMPILER} ${C_FLAGS} ${INCLUDES} ${SWITCHES} -c ${GENSRCDIR}/$*.c -o ${BUILDDIR}/$*.o

${BUILDDIR}/%.o: ${SRCDIR}/%.cpp Makefile ${SRCDIR}/*.hpp
	${CXX_COMPILER} ${CXX_FLAGS} ${INCLUDES} ${SWITCHES} -c ${SRCDIR}/$*.cpp -o ${BUILDDIR}/$*.o
clean:
	rm -f ${BUILDDIR}/*.o

distclean: clean
	rm -f ${GENSRCDIR}/xdg-shell-protocol.c
	rm -f ${GENSRCDIR}/xdg-shell-client-protocol.h
	rm -f ${GENSRCDIR}/zxdg-decoration-protocol.c
	rm -f ${GENSRCDIR}/zxdg-decoration-client-protocol.h

#---
# the app
#---

app: ${WAYLAND_HEADERS} ${OBJS} ${WAYLAND_OBJS}
	${LINKER} ${OBJS} ${WAYLAND_OBJS} -o ${APPNAME} ${LINK_LIBS}
