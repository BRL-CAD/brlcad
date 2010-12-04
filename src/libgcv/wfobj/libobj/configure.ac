AC_PREREQ(2.59)
AC_INIT(libobj, 0.5.2)
AC_CONFIG_SRCDIR(Makefile.am)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.                                                          
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_LIBTOOL_DLOPEN

AC_PROG_LIBTOOL
dnl Configure libltdl                                                           
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_LN_S

AC_DEFINE([SHARED_PTR_BOOST],[1],[shared_ptr is in <boost/shared_ptr.hpp>])
shared_ptr_header=boost

AC_CHECK_HEADER([unistd.h],[],
	[AC_DEFINE([YYNO_UNISTD_H],[1],[Don't include unistd.h])])
AC_CHECK_FUNC([read],[],
	[AC_DEFINE([NO_POSIX_READ],[1],[Don't use read function])])

BOOST_CPPFLAGS='-I${top_srcdir}/src/boost_shared_ptr'
AC_SUBST(BOOST_CPPFLAGS)

AC_CONFIG_FILES( \
 Makefile \
 src/Makefile
 src/tests/Makefile)

AC_OUTPUT
