# $Id$

SUBDIRS = include

A2G=$(top_builddir)/src/conv/asc2g
G2A=$(top_builddir)/src/conv/g2asc

.asc.g: ; $(A2G) $(builddir)$< $@

MODELS = \
	axis.asc \
	bldg391.asc \
	boolean-ops.asc \
	castle.asc \
	cornell.asc \
	cray.asc \
	crod.asc \
	cube.asc \
	demo.asc \
	galileo.asc \
	havoc.asc \
	kman.asc \
	ktank.asc \
	lgt-test.asc \
	m35.asc \
	moss.asc \
	pic.asc \
	prim.asc \
	sphflake.asc \
	star.asc \
	tank_car.asc \
	terra.asc \
	terra.asc \
	truck.asc \
	wave.asc \
	woodsman.asc \
	world.asc \
	xmp.asc

EXTRA_DIST = \
	${MODELS} \
	db.php

MAINTAINERCLEANFILES = Makefile.in

all: ${MODELS:asc=g}

install-data-hook: ${MODELS:asc=g}
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
	@list='${MODELS:asc=g}'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pkgdatadir)/$$f"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pkgdatadir)/$$f; \
	done
																					    
uninstall:
	@$(NORMAL_UNINSTALL)
	@list='${MODELS:asc=g}'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(pkgdatadir)/$$f"; \
	  rm -f $(DESTDIR)$(pkgdatadir)/$$f; \
	done

clean:
	@list='${MODELS:asc=g}'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $$f"; \
	  rm -f $$f; \
	done
