#                    C O N F I G U R E . A C
# enigma
#
# Published in 1998 by Joerg Wunsch
# This work is in the public domain.
#
# Implementations of crypt are very common among operating systems.
# This implementation of crypt, named enigma, was taken from the
# public domain Crypt Breaker's Workbench software.
###

AC_INIT(Enigma, 0.0.2, [http://brlcad.org], enigma)
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([enigma.c])
AM_INIT_AUTOMAKE([1.6 dist-zip dist-bzip2])
AM_CONFIG_HEADER([config.h])

# check programs
AC_PROG_CC

# check libraries
AC_CHECK_LIB(crypt, crypt)

dnl check for the getpass function in -lbsd and/or -lc
LIBBSD=""
AC_CHECK_LIB(bsd, getpass, LIBBSD="-lbsd",
    [AC_CHECK_LIB(c, getpass, LIBBSD="-lc")]
)
AC_SUBST(LIBBSD)

# do your thing
AC_OUTPUT(Makefile)

# Local Variables:
# tab-width: 8
# mode: autoconf
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
