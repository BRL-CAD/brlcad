
if WITH_X11
IR_X_BIN=ir-X
IRDISP_BIN=irdisp
else
IR_X_EXTRA=ir-X.c
IRDISP_EXTRA=irdisp.c
endif

bin_PROGRAMS = \
	all_sf \
	showtherm \
	firpass \
	${IR_X_BIN} \
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

EXTRA_DIST = \
	${IRDISP_EXTRA} \
	${IR_X_EXTRA} \
	pictx.c \
	CMakeLists.txt

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(all_sf_OBJECTS) \
	$(showtherm_OBJECTS) \
	$(firpass_OBJECTS) \
	$(ir_X_OBJECTS) \
	$(irdisp_OBJECTS) \
	$(secpass_OBJECTS) \
	$(shapefact_OBJECTS) \
	$(bin_PROGRAMS)
