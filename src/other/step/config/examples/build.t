
set path=(. \
	/itl/links/spro/bin \
	/home/sauderd/src/pdevel/src/tkdp \
	/depot/x11r5/plat/bin	\
	$OPENWINHOME/bin		\
	$OPENWINHOME/demo		\
	~/bin \
	/depot/xframe5.0.1/bin	\
	/depot/unix/bin \
	/depot/bin \
	/depot/usenet/arch/bin \
	/depot/ncd/binSun4 \
	/proj/framewrk/src/ostore/cfront/debug/bin \
	/depot/OStore4.0/arch/sunpro/bin \
	/depot/X11R4/arch/bin \
	/depot/pub/arch/bin \
	/ignore/pdevel/arch/bin \
	/depot/cygnus/arch/bin \
	/depot/gnu/arch/bin \
	/depot/sundry/arch/bin \
	/depot/sparcworks.new \
	/depot/sparcworks.new/bin \
	/depot/centerline/bin \
	$FMHOME/bin \
	/depot/athena/arch/bin \
	/depot/doswindows/arch/bin \
	/usr/ucb /bin /usr/bin /etc /usr/games /usr/etc \
	/usr/bsd /usr/bin/X11 \
	/depot/netmedia/plat/bin /depot/acrobat/plat/bin /depot/www/bin \
	/depot/tcl/arch/bin \
	/depot/tcl/bin \
	/proj/apde/pure \
	/depot/pub/arch/bin \
	/usr/sbin \
	/usr/local/bin \
	/usr/ccs/bin \
	/proj/pdevel/src/STEP.solaris/bin \
)

set path=($path /depot/netmedia/plat/bin /depot/acrobat/plat/bin )

unsetenv LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH /itl/links/spro/lib:/usr/lib:/depot/OStore4.0/arch.sunos4/sunpro/lib:/depot/x11r5/plat/lib:/depot/openwin3/lib

unsetenv LEX
#setenv LEX /itl/links/spro/bin/lex
setenv LEX lex
#setenv LEX /depot/gnu/arch/bin/flex

unsetenv YACC
#setenv YACC byacc
setenv YACC /itl/links/spro/bin/yacc
#setenv YACC yacc
#setenv YACC /depot/gnu/arch/bin/bison

unsetenv CC
setenv CC /itl/links/spro/bin/acc
unsetenv CXX
setenv CXX /itl/links/spro/bin/CC
configure --with-arch=archt --with-iv=/proj/pdeseval/iv3.1_SUN.C++.SOLARIS --with-iv-includes=/proj/pdeseval/iv3.1_SUN.C++.SOLARIS/src/include --with-iv-lib=/proj/pdeseval/iv3.1_SUN.C++.SOLARIS/src/lib/IV/SUN4
