#                        A R G S . M 4
# BRL-CAD
#
# Copyright (C) 2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above 
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# BC_ARG_ENABLE
#
# creates a configure --enable argument that simply sets a variable as
# to whether the feature is enabled or not.  The arguments expected
# include the variable name, the --enable-FEATURE name, a help string,
# and a default value.
#
#
# BC_ARG_ALIAS
#
# creates an --enable alias.  argument aliases do not show up in help,
# so the only expected arguments to the macro is the variable name,
# and the FEATURE alias name.
# 
#
# BC_ARG_WITH
#
# creates a configure --with argument setting one FEATURE variable if
# the value was with or without (yes/no) and another FEATURE_var for
# the actual value.
#
#
# BC_ARG_WITH_ALIAS
#
# same as BC_ARG_ALIAS except that it sets the FEATURE_var variable
# too.  BC_ARG_ALIAS will work as a simplified alternative if the
# FEATURE_var variable is not used (i.e. only a yes/no matters)
#
#
# BC_WITH_FLAG_ARGS
#
# provides convenience argument handlers for specifying CFLAGS,
# LDFLAGS, CPPFLAGS, and LIBS.  more specifically, it adds
# --with-cflags, --with-cppflags, --with-ldflags, --with-libs.
#
###

AC_DEFUN([BC_ARG_ENABLE], [

dnl XXX this was a failed attempt to get AC_ARG_ENABLE to grok an argument name
dnl ifelse([$1], [], [errprint([ERROR: missing first argument (the variable name) to BC_ARG_ENABLE])])
dnl _bc_arg_name=ifelse([$2], [], [$1], [$2])
dnl _bc_arg_help=ifelse([$4], [],
dnl	[ifelse([$3], [], [enable $2], [$3])],
dnl	[ifelse([$3], [], [enable $2 (default=$4)], [$3 (default=$4)] )]
dnl)

# BC_ARG_ENABLE 1:[$1] 2:[$2] 3:[$3] 4:[$4]
bc_[$1]=[$4]
AC_ARG_ENABLE([$2], AC_HELP_STRING([--enable-$2], [$3 (default=$4)]), 
	[
	case "x$enableval" in
		x[[yY]][[eE]][[sS]])
			bc_[$1]=yes
			;;
		x[[nN]][[oO]])
			bc_[$1]=no
			;;
		x)
			bc_[$1]=yes
			;;
		x*)
			AC_MSG_NOTICE([*** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ***])
			AC_MSG_NOTICE([Unexpected value of [$enableval] to --enable-[$2] (expecting yes/no)])
			bc_[$1]="$enableval"
			;;
	esac
	]
)
])


AC_DEFUN([BC_ARG_ALIAS], [
# BC_ARG_ALIAS 1:[$1] 2:[$2]
AC_ARG_ENABLE([$2],,
	[
	case "x$enableval" in
		x[[yY]][[eE]][[sS]])
			bc_[$1]=yes
			;;
		x[[nN]][[oO]])
			bc_[$1]=no
			;;
		x)
			bc_[$1]=yes
			;;
		x*)
			bc_[$1]="$enableval"
			;;
	esac
	]
)
])



AC_DEFUN([BC_ARG_WITH], [
# BC_ARG_WITH 1:[$1] 2:[$2] 3:[$3] 4:[$4] 5:[$5]
bc_[$1]=[$4]
bc_[$1]_val=[$5]
AC_ARG_WITH([$2], AC_HELP_STRING([--with-$2], [$3]), 
	[
	case "x$withval" in
		x[[yY]][[eE]][[sS]])
			bc_[$1]=yes
			bc_[$1]_val=yes
			;;
		x[[nN]][[oO]])
			bc_[$1]=no
			bc_[$1]_val=no
			;;
		x)
			bc_[$1]=yes
			bc_[$1]_val="$withval"
			;;
		x*)
			bc_[$1]=yes
			bc_[$1]_val="$withval"
			;;
	esac
	]
)
])


AC_DEFUN([BC_ARG_WITH_ALIAS], [
# BC_ARG_WITH_ALIAS 1:[$1] 2:[$2]
AC_ARG_WITH([$2],,
	[
	case "x$withval" in
		x[[yY]][[eE]][[sS]])
			bc_[$1]=yes
			bc_[$1]_val=yes
			;;
		x[[nN]][[oO]])
			bc_[$1]=no
			bc_[$1]_val=no
			;;
		x)
			bc_[$1]=yes
			bc_[$1]_val="$withval"
			;;
		x*)
			bc_[$1]=yes
			bc_[$1]_val="$withval"
			;;
	esac
	]
)
])


AC_DEFUN([BC_WITH_FLAG_ARGS], [
AC_ARG_WITH(cflags, AC_HELP_STRING(--with-cflags,
		[Specify additional flags to pass to the C compiler]),
	[
		if test "x$withval" != "xno" ; then
			CFLAGS="$CFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(cppflags, AC_HELP_STRING(--with-cppflags,
		[Specify additional flags to pass to C preprocessor]),
	[
		if test "x$withval" != "xno"; then
			CPPFLAGS="$CPPFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(ldflags, AC_HELP_STRING(--with-ldflags,
		[Specify additional flags to pass to linker]),
	[
		if test "x$withval" != "xno" ; then
			LDFLAGS="$LDFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(libs, AC_HELP_STRING(--with-libs,
		[Specify additional libraries to link against]),
	[
		if test "x$withval" != "xno" ; then
			LIBS="$LIBS $withval"
		fi
	]	
)
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
