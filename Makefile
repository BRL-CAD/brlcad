#################################################################
#								#
#			Makefile				#
#								#
# The Master Makefile for the BRL CAD Software Distribution	#
#								#
#  Author -							#
#	Michael John Muuss					#
#	BRL/SECAD, August 1986					#
#								#
# Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005-5066		#
#								#
#  $Header$		#
#								#
#################################################################

SHELL		= /bin/sh

# Label number for this CAD Release
RELEASE		= 2.4
# RCS main Revision number
RCS_REVISION	= 8
# Release 1.20 was Revision 1
# Release 1.24 was Revision 5
# Release 2.0 was Revision 6
# Release 2.3 was Revision 7

DISTDIR		= /m/dist/.
ARCHDIR		= /m/.
ARCHIVE		= ${ARCHDIR}/cad${RELEASE}.tar

TOP_FILES	= README Makefile Makefile.defs Makefile.rules \
		cray.sh cray-ar.sh

# NOTE:  The following directories contain code which may not compile
# on all systems.  In that case, they should be removed from $DIRS:
#
#	libpkg		BSD or SGI networking, remove if SYSV
#	rfbd		BSD or SGI networking, remove if SYSV
#
DIRS		= h \
		  doc \
		  libsysv \
		  libmalloc \
		  conv bench \
		  db pix \
		  libpkg \
		  libfb \
		  rfbd \
		  libtermio \
		  libcursor \
		  libplot3 \
		  libtig \
		  libwdb \
		  librle \
		  libspl librt rt \
		  mged \
		  proc-db \
		  util \
		  fbed \
		  lgt \
		  vdeck \
		  whetstone dhrystone

all:
	@echo; echo "Release ${RELEASE}.  make $@"
	-@for dir in ${DIRS}; \
	do	echo " "; \
		echo ---------- $$dir; \
		(cd $$dir; make depend; make -k )\
	done

benchmark:
	@echo; echo "Release ${RELEASE}.  make $@"
	cd libsysv; make depend; make -k
	cd libmalloc; make depend; make -k
	cd conv; make -k
	cd db; make -k
	cd pix; make -k
	cd libpkg; make depend; make -k	  # needed for IF_REMOTE, rm if SYSV
	cd libfb; make depend; make -k
	cd libplot3; make depend; make -k
	cd libspl; make depend; make -k
	cd librt; make depend; make -k
	cd rt; make depend; make -k

depend:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k depend) \
	done

install:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k install) \
	done

inst-man:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k inst-man) \
	done

uninstall:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k uninstall) \
	done

print:
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k print) \
	done

typeset:
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k typeset) \
	done

# BRL-only:  install sources in distribution tree without installing products
inst-dist:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k inst-dist) \
	done

checkin:
	@echo; echo "Release ${RELEASE}, RCS_Revision ${RCS_REVISION}.  make $@"
	rcs -l ${TOP_FILES}
	ci -u -f -r${RCS_REVISION} -sRel "-MRelease ${RELEASE}" ${TOP_FILES}
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; rm -f Makefile.bak; \
		rcs -l *.[cshf1-9] Makefile*; \
		ci -u -f -r${RCS_REVISION} -sRel "-MRelease ${RELEASE}" *.[cshf1-9] Makefile* ) \
	done

# Remove all binary files (clean, noprod)
clobber:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k clobber) \
	done

# Remove all .o files, leave products
clean:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k clean) \
	done

# Remove all products, leave all .o files
noprod:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k noprod) \
	done

lint:
	@echo; echo "Release ${RELEASE}.  make $@"
	-for dir in ${DIRS}; \
	do	echo ---------- $$dir; \
		(cd $$dir; make -k lint) \
	done

# Do a "make install" as step 1 of making a distribution.
#
# Use "make dist" as step 2 of making a distribution.
dist:
	@echo; echo "Release ${RELEASE}.  make $@"
	cp Copyright* README Makefile* cray.sh cray-ar.sh ${DISTDIR}
	(cd bench; make clobber; make install)
	cd ${DISTDIR}; du -a > Contents

# Use as step 3 of making a distribution -- write the archive
arch:
	@echo; echo "Release ${RELEASE}.  make $@"
	cd ${DISTDIR}; tar cfv ${ARCHIVE} *
	# $4 will be file size in bytes, pad to 200K byte boundary for SGI
	set -- `ls -l ${ARCHIVE}` ; \
		PADBYTES=`echo "204800 $4 204800 %-p" | dc` ; \
		if test ${PADBYTES} -lt 204800 ; then \
			gencolor -r${PADBYTES} 0 >> ${ARCHIVE} ; \
		fi
	chmod 444 ${ARCHIVE}


# Here, we assume that all the machine-specific files have been set up.
#
#HOSTS=brl-vmb brl-sem
#
#rdist:	# all
#	for host in ${HOSTS} ; do (rdist -d HOSTS=$$host all ; \
#		rsh $$host "(cd cad; make -k all)" ) \
#		; done
#		| mail mike -s CAD_Update ; done
