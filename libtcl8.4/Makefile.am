# $Id$

configure:
	mkdir -p build
	(cd build && \
		${top_srcdir}/../libtcl8.4/unix/configure  \
		--srcdir=${top_srcdir}/../libtcl8.4/unix \
		--enable-shared \
		--exec-prefix=${prefix} \
		--prefix=${prefix} \
		--x-includes=@X_PREFIX@/include \
		--x-libraries=@X_PREFIX@/lib \
		--with-tcl=${top_builddir}/../libtcl8.4\
	)

all-am:
	if [ ! -d build ] ; then $(MAKE) configure ; fi
	if [ ! -e build/Makefile ] ; then rm -rf build && $(MAKE) configure ; fi
	(cd build && $(MAKE) -$(MAKEFLAGS))

install-am:
	(cd build && $(MAKE) -$(MAKEFLAGS) install)

clean-am:
	rm -rf build
