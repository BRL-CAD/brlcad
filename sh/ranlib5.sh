#!/bin/sh
# System V substitute for RANLIB which does not blather on stdout
#  $Header$

ar ts $1 2>&1 >/dev/null

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
