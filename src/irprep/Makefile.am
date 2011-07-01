
if WITH_X11
IR_X_BIN=ir-X
IR_X_MAN=ir-X.1
IRDISP_BIN=irdisp
IRDISP_MAN=irdisp.1
else
IR_X_EXTRA=ir-X.c ir-X.1
IRDISP_EXTRA=irdisp.c irdisp.1
endif

bin_PROGRAMS = \
	all_sf \
	showtherm \
	firpass \
	${IR_X_BIN} \
	ir-sgi \
	${IRDISP_BIN} \
	secpass \
	shapefact

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

all_sf_SOURCES = all_sf.c
all_sf_LDADD = ${RT}

showtherm_SOURCES = showtherm.c
showtherm_LDADD = ${RT}

firpass_SOURCES = firpass.c subroutines.c
firpass_LDADD = ${RT}

if WITH_X11
irdisp_SOURCES = irdisp.c
irdisp_CFLAGS = ${X_CFLAGS}
irdisp_LDADD = \
	${BU} \
	${X_LIBS}
endif

ir_sgi_SOURCES = ir-sgi.c
ir_sgi_LDADD = ${BU}

if WITH_X11
ir_X_SOURCES = ir-X.c
ir_X_CFLAGS = ${X_CFLAGS}
ir_X_LDADD = \
	${BU} \
	${X_LIBS}
endif

secpass_SOURCES = secpass.c subroutines.c
secpass_LDADD = ${RT}

shapefact_SOURCES = shapefact.c
shapefact_LDADD = ${RT}

man_MANS = \
	showtherm.1 \
	firpass.1 \
	${IRDISP_MAN} \
	ir-sgi.1 \
	${IR_X_MAN} \
	secpass.1 \
	shapefact.1

EXTRA_DIST = \
	$(man_MANS) \
	${IRDISP_EXTRA} \
	${IR_X_EXTRA} \
	Compile.sgi \
	pictsgi.1 \
	pictsgi.c \
	pictx.1 \
	pictx.c \
	CMakeLists.txt

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(all_sf_OBJECTS) \
	$(showtherm_OBJECTS) \
	$(firpass_OBJECTS) \
	$(ir_X_OBJECTS) \
	$(ir_sgi_OBJECTS) \
	$(irdisp_OBJECTS) \
	$(secpass_OBJECTS) \
	$(shapefact_OBJECTS) \
	$(bin_PROGRAMS)
