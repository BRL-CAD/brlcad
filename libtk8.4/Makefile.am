# $Id$

configure:
	mkdir -p build
	(cd build && \
		${top_srcdir}/../libtk8.4/unix/configure  \
		--srcdir=${top_srcdir}/../libtk8.4/unix \
		--enable-shared \
		--exec-prefix=${prefix} \
		--prefix=${prefix} \
		--x-includes=@X_PREFIX@/include \
		--x-libraries=@X_PREFIX@/lib \
		--with-tcl=${top_builddir}/../libtcl8.4/build\
	)


all-am:
	if [ ! -d build ] ; then make configure ; fi
	if [ ! -e build/Makefile ] ; then rm -rf build && make configure ; fi
	$(MAKE) -$(MAKEFLAGS) -C build all

install-am:
	$(MAKE) -$(MAKEFLAGS) -C build install

clean-am:
	rm -rf build
