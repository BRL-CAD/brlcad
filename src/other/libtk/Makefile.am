# $Id$

configure:
	mkdir -p build
	(cd build && \
		${top_srcdir}/../src/other/libtk/unix/configure  \
		--srcdir=${top_srcdir}/../src/other/libtk/unix \
		--enable-shared \
		--exec-prefix=${prefix} \
		--prefix=${prefix} \
		--x-includes=@X_PREFIX@/include \
		--x-libraries=@X_PREFIX@/lib \
		--with-tcl=${top_builddir}/../src/other/libtcl/unix \
	)


all-am:
	if [ ! -d build ] ; then $(MAKE) configure ; fi
	if [ ! -e build/Makefile ] ; then rm -rf build && $(MAKE) configure ; fi
	$(MAKE) -$(MAKEFLAGS) -C build all

install-am: all-am
	$(MAKE) -$(MAKEFLAGS) -C build install

clean-am:
	rm -rf build
