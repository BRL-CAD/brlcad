################################################################################
#
# Simple makefile for building on Mac OS X with the
# Project Builder command line tool 'pbxbuild'
#
# RCS: @(#) $Id$
#
################################################################################

INSTALL_ROOT	?=

BUILD_DIR	?= ${CURDIR}/../../build

TARGET		= Wish

DEVBUILDSTYLE	= Development
DEPBUILDSTYLE	= Deployment

PBXBUILD	= /usr/bin/pbxbuild

BUILD		= ${PBXBUILD} SYMROOT="${BUILD_DIR}" -target "${TARGET}"

DEVBUILD	= ${BUILD} -buildstyle "${DEVBUILDSTYLE}"
DEPBUILD	= ${BUILD} -buildstyle "${DEPBUILDSTYLE}"

INSTALLOPTS	= INSTALL_ROOT="${INSTALL_ROOT}"

EMBEDDEDOPTS	= EMBEDDED_BUILD=1

################################################################################

all: develop deploy

install: install-develop install-deploy

embedded: embedded-deploy

install-embedded: install-embedded-deploy cleanup-embedded

clean: clean-develop clean-deploy

################################################################################

develop:
	${DEVBUILD}

deploy:
	${DEPBUILD}

install-develop: 
	${DEVBUILD} install ${INSTALLOPTS}

install-deploy:
	${DEPBUILD} install ${INSTALLOPTS}

embedded-develop:
	${DEVBUILD} ${EMBEDDEDOPTS}

embedded-deploy:
	${DEPBUILD} ${EMBEDDEDOPTS}

install-embedded-develop:
	${DEVBUILD} install ${INSTALLOPTS} ${EMBEDDEDOPTS}

install-embedded-deploy:
	${DEPBUILD} install ${INSTALLOPTS} ${EMBEDDEDOPTS}

clean-develop:
	${DEVBUILD} clean

clean-deploy:
	${DEPBUILD} clean

################################################################################

forceRelink:
	@-cd ${BUILD_DIR}; \
	rm -rf Tk.framework Wish\ Shell.app libtkstub8.4.a

cleanup-embedded:
	@-cd ${INSTALL_ROOT}; \
	chmod -RH u+w Library/Frameworks/Tcl.framework; \
	rm -rf Library/Frameworks/Tcl.framework; \
	chmod -RH u+w Library/Frameworks/Tk.framework; \
	rm -rf Library/Frameworks/Tk.framework; \
	rmdir -p Library/Frameworks 2>&-;

################################################################################

.PHONY: all install embedded clean develop deploy install-develop install-deploy \
embedded-develop embedded-deploy install-embedded-develop install-embedded-deploy \
clean-develop clean-deploy forceRelinkTcl \
cleanup-embedded

################################################################################
