# This was taken from Tk's configure.in and tcl.m4.
AC_DEFUN([TREECTRL_PROG_RC], [

#    AC_MSG_CHECKING([for windows resource compiler])
    RC_DEPARG='"$<"'
    if test "${GCC}" = "yes" ; then
	AC_CHECK_PROG(RC, windres, windres)
	if test "${RC}" = "" ; then
	    AC_MSG_ERROR([Required resource tool 'windres' not found on PATH.])
	fi
	RC_OUT=-o
	RC_TYPE=
	RC_INCLUDE=--include
	RC_DEFINE=--define
	RES=res.o

	# Check for a bug in gcc's windres that causes the
	# compile to fail when a Windows native path is
	# passed into windres. The mingw toolchain requires
	# Windows native paths while Cygwin should work
	# with both. Avoid the bug by passing a POSIX
	# path when using the Cygwin toolchain.

	if test "$ac_cv_cygwin" != "yes" -a "$CYGPATH" != "echo" ; then
	    conftest=/tmp/conftest.rc
	    echo "STRINGTABLE BEGIN" > $conftest
	    echo "101 \"name\"" >> $conftest
	    echo "END" >> $conftest

	    AC_MSG_CHECKING([for Windows native path bug in windres])
	    cyg_conftest=`$CYGPATH $conftest`
	    if AC_TRY_COMMAND($RC -o conftest.res.o $cyg_conftest) ; then
		AC_MSG_RESULT([no])
		RC_DEPARG='"$(shell $(CYGPATH) $<)"'
	    else
		AC_MSG_RESULT([yes])
	    fi
	    conftest=
	    cyg_conftest=
	fi

    else
	if test "$do64bit" != "no" ; then
	    RC="\"${MSSDK}/bin/rc.exe\""
	elif test "$doWince" != "no" ; then
	    RC="\"${WCEROOT}/Common/EVC/bin/rc.exe\""
	else
	    RC="rc"
	fi
	RC_OUT=-fo
	RC_TYPE=-r
	RC_INCLUDE=-i
	RC_DEFINE=-d
	RES=res
    fi

    AC_SUBST(RC)
    AC_SUBST(RC_OUT)
    AC_SUBST(RC_TYPE)
    AC_SUBST(RC_INCLUDE)
    AC_SUBST(RC_DEFINE)
    AC_SUBST(RC_DEPARG)
    AC_SUBST(RES)
])

# This is basically TEA_ADD_SOURCES but sets the object file extension to
# $RES instead of $OBJ.
AC_DEFUN([TREECTRL_ADD_RC], [
    vars="$@"
    for i in $vars; do
	case $i in
	    [\$]*)
		# allow $-var names
		PKG_SOURCES="$PKG_SOURCES $i"
		PKG_OBJECTS="$PKG_OBJECTS $i"
		;;
	    *)
		# check for existence - allows for generic/win/unix VPATH
		if test ! -f "${srcdir}/$i" -a ! -f "${srcdir}/generic/$i" \
		    -a ! -f "${srcdir}/win/$i" -a ! -f "${srcdir}/unix/$i" \
		    ; then
		    AC_MSG_ERROR([could not find source file '$i'])
		fi
		PKG_SOURCES="$PKG_SOURCES $i"
		# this assumes it is in a VPATH dir
		i=`basename $i`
		# handle user calling this before or after TEA_SETUP_COMPILER
		if test x"${RES}" != x ; then
		    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.${RES}"
		else
		    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.\${RES}"
		fi
		PKG_OBJECTS="$PKG_OBJECTS $j"
		TEA_ADD_CLEANFILES($j)
		;;
	esac
    done
    AC_SUBST(PKG_SOURCES)
    AC_SUBST(PKG_OBJECTS)
])
