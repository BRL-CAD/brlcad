# $Id$

	 
A2G=$(top_builddir)/conv/asc2g
G2A=$(top_builddir)/conv/g2asc

#SUFFIXES: .g .asc

.asc.g: ; $(A2G) $(builddir)$< $@

UH = Makefile.dist db.php
MODELS = axis.asc bldg391.asc boolean-ops.asc castle.asc cornell.asc cray.asc crod.asc cube.asc demo.asc galileo.asc havoc.asc kman.asc ktank.asc lgt-test.asc m35.asc moss.asc pic.asc prim.asc sphflake.asc star.asc tank_car.asc terra.asc terra.dsp truck.asc wave.asc woodsman.asc world.asc xmp.asc

EXTRA_DIST = ${UH} ${MODELS}

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

