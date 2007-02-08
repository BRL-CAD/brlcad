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

AC_INIT(enigma.c)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(enigma, 0.0.2)
AM_CONFIG_HEADER(config.h)

# check programs
AC_PROG_CC

# check libraries
AC_CHECK_LIB(crypt, crypt)

# do your thing
AC_OUTPUT(Makefile)

# Local Variables:
# tab-width: 8
# mode: autoconf
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
