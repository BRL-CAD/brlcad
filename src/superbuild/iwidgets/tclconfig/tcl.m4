# tcl.m4 --
#
#	This file provides a set of autoconf macros to help TEA-enable
#	a Tcl extension.
#
# Copyright (c) 1999-2000 Ajuba Solutions.
# Copyright (c) 2002-2005 ActiveState Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id$

AC_PREREQ(2.50)

#------------------------------------------------------------------------
# TEA_PATH_TCLCONFIG --
#
#	Locate the tclConfig.sh file and perform a sanity check on
#	the Tcl compile flags
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tcl=...
#
#	Defines the following vars:
#		TCL_BIN_DIR	Full path to the directory containing
#				the tclConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_TCLCONFIG, [
    dnl Make sure we are initialized
    AC_REQUIRE([TEA_INIT])
    #
    # Ok, lets find the tcl configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tcl
    #

    if test x"${no_tcl}" = x ; then
	# we reset no_tcl in case something fails here
	no_tcl=true
	AC_ARG_WITH(tcl, [  --with-tcl              directory containing tcl configuration (tclConfig.sh)], with_tclconfig=${withval})
	AC_MSG_CHECKING([for Tcl configuration])
	AC_CACHE_VAL(ac_cv_c_tclconfig,[

	    # First check to see if --with-tcl was specified.
	    if test x"${with_tclconfig}" != x ; then
		case ${with_tclconfig} in
		    */tclConfig.sh )
			if test -f ${with_tclconfig}; then
			    AC_MSG_WARN([--with-tcl argument should refer to directory containing tclConfig.sh, not to tclConfig.sh itself])
			    with_tclconfig=`echo ${with_tclconfig} | sed 's!/tclConfig\.sh$!!'`
			fi ;;
		esac
		if test -f "${with_tclconfig}/tclConfig.sh" ; then
		    ac_cv_c_tclconfig=`(cd ${with_tclconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_tclconfig} directory doesn't contain tclConfig.sh])
		fi
	    fi

	    # then check for a private Tcl installation
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			../tcl \
			`ls -dr ../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tcl \
                        `ls -dr ../../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
                        `ls -dr ../../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tcl \
                        `ls -dr ../../../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
                        `ls -dr ../../../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tclConfig.sh" ; then
			ac_cv_c_tclconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/tclConfig.sh" ; then
			ac_cv_c_tclconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few other private locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			${srcdir}/../tcl \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tclConfig.sh" ; then
		    ac_cv_c_tclconfig=`(cd $i/unix; pwd)`
		    break
		fi
		done
	    fi
	])

	if test x"${ac_cv_c_tclconfig}" = x ; then
	    TCL_BIN_DIR="# no Tcl configs found"
	    AC_MSG_WARN("Cannot find Tcl configuration definitions")
	    exit 0
	else
	    no_tcl=
	    TCL_BIN_DIR=${ac_cv_c_tclconfig}
	    AC_MSG_RESULT([found $TCL_BIN_DIR/tclConfig.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_PATH_TKCONFIG --
#
#	Locate the tkConfig.sh file
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tk=...
#
#	Defines the following vars:
#		TK_BIN_DIR	Full path to the directory containing
#				the tkConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_TKCONFIG, [
    #
    # Ok, lets find the tk configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tk
    #

    if test x"${no_tk}" = x ; then
	# we reset no_tk in case something fails here
	no_tk=true
	AC_ARG_WITH(tk, [  --with-tk               directory containing tk configuration (tkConfig.sh)], with_tkconfig=${withval})
	AC_MSG_CHECKING([for Tk configuration])
	AC_CACHE_VAL(ac_cv_c_tkconfig,[

	    # First check to see if --with-tkconfig was specified.
	    if test x"${with_tkconfig}" != x ; then
		case ${with_tkconfig} in
		    */tkConfig.sh )
			if test -f ${with_tkconfig}; then
			    AC_MSG_WARN([--with-tk argument should refer to directory containing tkConfig.sh, not to tkConfig.sh itself])
			    with_tkconfig=`echo ${with_tkconfig} | sed 's!/tkConfig\.sh$!!'`
			fi ;;
		esac
		if test -f "${with_tkconfig}/tkConfig.sh" ; then
		    ac_cv_c_tkconfig=`(cd ${with_tkconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_tkconfig} directory doesn't contain tkConfig.sh])
		fi
	    fi

	    # then check for a private Tk library
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			../tk \
			`ls -dr ../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tk \
			`ls -dr ../../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tk \
			`ls -dr ../../../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi
	    # check in a few common install locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	    # check in a few other private locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			${srcdir}/../tk \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi
	])
	if test x"${ac_cv_c_tkconfig}" = x ; then
	    TK_BIN_DIR="# no Tk configs found"
	    AC_MSG_WARN("Cannot find Tk configuration definitions")
	    exit 0
	else
	    no_tk=
	    TK_BIN_DIR=${ac_cv_c_tkconfig}
	    AC_MSG_RESULT([found $TK_BIN_DIR/tkConfig.sh])
	fi
    fi

])

#------------------------------------------------------------------------
# TEA_LOAD_TCLCONFIG --
#
#	Load the tclConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		TCL_BIN_DIR
#
# Results:
#
#	Subst the following vars:
#		TCL_BIN_DIR
#		TCL_SRC_DIR
#		TCL_LIB_FILE
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_TCLCONFIG, [
    AC_MSG_CHECKING([for existence of $TCL_BIN_DIR/tclConfig.sh])

    if test -f "$TCL_BIN_DIR/tclConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. $TCL_BIN_DIR/tclConfig.sh
    else
        AC_MSG_RESULT([file not found])
    fi

    #
    # If the TCL_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TCL_LIB_SPEC will be set to the value
    # of TCL_BUILD_LIB_SPEC. An extension should make use of TCL_LIB_SPEC
    # instead of TCL_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f $TCL_BIN_DIR/Makefile ; then
        TCL_LIB_SPEC=${TCL_BUILD_LIB_SPEC}
        TCL_STUB_LIB_SPEC=${TCL_BUILD_STUB_LIB_SPEC}
        TCL_STUB_LIB_PATH=${TCL_BUILD_STUB_LIB_PATH}
    fi

    #
    # eval is required to do the TCL_DBGX substitution
    #

    eval "TCL_LIB_FILE=\"${TCL_LIB_FILE}\""
    eval "TCL_LIB_FLAG=\"${TCL_LIB_FLAG}\""
    eval "TCL_LIB_SPEC=\"${TCL_LIB_SPEC}\""

    eval "TCL_STUB_LIB_FILE=\"${TCL_STUB_LIB_FILE}\""
    eval "TCL_STUB_LIB_FLAG=\"${TCL_STUB_LIB_FLAG}\""
    eval "TCL_STUB_LIB_SPEC=\"${TCL_STUB_LIB_SPEC}\""

    AC_SUBST(TCL_VERSION)
    AC_SUBST(TCL_BIN_DIR)
    AC_SUBST(TCL_SRC_DIR)

    AC_SUBST(TCL_LIB_FILE)
    AC_SUBST(TCL_LIB_FLAG)
    AC_SUBST(TCL_LIB_SPEC)

    AC_SUBST(TCL_STUB_LIB_FILE)
    AC_SUBST(TCL_STUB_LIB_FLAG)
    AC_SUBST(TCL_STUB_LIB_SPEC)

    AC_SUBST(TCL_LIBS)
    AC_SUBST(TCL_DEFS)
    AC_SUBST(TCL_EXTRA_CFLAGS)
    AC_SUBST(TCL_LD_FLAGS)
    AC_SUBST(TCL_SHLIB_LD_LIBS)
    #AC_SUBST(TCL_BUILD_LIB_SPEC)
    #AC_SUBST(TCL_BUILD_STUB_LIB_SPEC)
])

#------------------------------------------------------------------------
# TEA_LOAD_TKCONFIG --
#
#	Load the tkConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		TK_BIN_DIR
#
# Results:
#
#	Sets the following vars that should be in tkConfig.sh:
#		TK_BIN_DIR
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_TKCONFIG, [
    AC_MSG_CHECKING([for existence of ${TK_BIN_DIR}/tkConfig.sh])

    if test -f "${TK_BIN_DIR}/tkConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. $TK_BIN_DIR/tkConfig.sh
    else
        AC_MSG_RESULT([could not find ${TK_BIN_DIR}/tkConfig.sh])
    fi

    #
    # If the TK_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TK_LIB_SPEC will be set to the value
    # of TK_BUILD_LIB_SPEC. An extension should make use of TK_LIB_SPEC
    # instead of TK_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f $TK_BIN_DIR/Makefile ; then
        TK_LIB_SPEC=${TK_BUILD_LIB_SPEC}
        TK_STUB_LIB_SPEC=${TK_BUILD_STUB_LIB_SPEC}
        TK_STUB_LIB_PATH=${TK_BUILD_STUB_LIB_PATH}
    fi

    #
    # eval is required to do the TK_DBGX substitution
    #

    eval "TK_LIB_FILE=\"${TK_LIB_FILE}\""
    eval "TK_LIB_FLAG=\"${TK_LIB_FLAG}\""
    eval "TK_LIB_SPEC=\"${TK_LIB_SPEC}\""

    eval "TK_STUB_LIB_FILE=\"${TK_STUB_LIB_FILE}\""
    eval "TK_STUB_LIB_FLAG=\"${TK_STUB_LIB_FLAG}\""
    eval "TK_STUB_LIB_SPEC=\"${TK_STUB_LIB_SPEC}\""

    AC_SUBST(TK_VERSION)
    AC_SUBST(TK_BIN_DIR)
    AC_SUBST(TK_SRC_DIR)

    AC_SUBST(TK_LIB_FILE)
    AC_SUBST(TK_LIB_FLAG)
    AC_SUBST(TK_LIB_SPEC)

    AC_SUBST(TK_STUB_LIB_FILE)
    AC_SUBST(TK_STUB_LIB_FLAG)
    AC_SUBST(TK_STUB_LIB_SPEC)

    AC_SUBST(TK_LIBS)
    AC_SUBST(TK_XINCLUDES)
])

#------------------------------------------------------------------------
# TEA_ENABLE_SHARED --
#
#	Allows the building of shared libraries
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-shared=yes|no
#
#	Defines the following vars:
#		STATIC_BUILD	Used for building import/export libraries
#				on Windows.
#
#	Sets the following vars:
#		SHARED_BUILD	Value of 1 or 0
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_SHARED, [
    AC_MSG_CHECKING([how to build libraries])
    AC_ARG_ENABLE(shared,
	[  --enable-shared         build and link with shared libraries [--enable-shared]],
	[tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_shared+set}" = set; then
	enableval="$enable_shared"
	tcl_ok=$enableval
    else
	tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
	AC_MSG_RESULT([shared])
	SHARED_BUILD=1
    else
	AC_MSG_RESULT([static])
	SHARED_BUILD=0
	AC_DEFINE(STATIC_BUILD)
    fi
    AC_SUBST(SHARED_BUILD)
])

#------------------------------------------------------------------------
# TEA_ENABLE_THREADS --
#
#	Specify if thread support should be enabled.  If "yes" is
#	specified as an arg (optional), threads are enabled by default.
#	TCL_THREADS is checked so that if you are compiling an extension
#	against a threaded core, your extension must be compiled threaded
#	as well.
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-threads
#
#	Sets the following vars:
#		THREADS_LIBS	Thread library(s)
#
#	Defines the following vars:
#		TCL_THREADS
#		_REENTRANT
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_THREADS, [
    AC_ARG_ENABLE(threads, [  --enable-threads        build with threads],
	[tcl_ok=$enableval], [tcl_ok=$1])

    if test "$tcl_ok" = "yes" -o "${TCL_THREADS}" = 1; then
	TCL_THREADS=1

	if test "${TEA_PLATFORM}" != "windows" ; then
	    # We are always OK on Windows, so check what this platform wants.
	    AC_DEFINE(USE_THREAD_ALLOC)
	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_THREAD_SAFE)
	    AC_CHECK_LIB(pthread,pthread_mutex_init,tcl_ok=yes,tcl_ok=no)
	    if test "$tcl_ok" = "no"; then
		# Check a little harder for __pthread_mutex_init in the
		# same library, as some systems hide it there until
		# pthread.h is defined.	 We could alternatively do an
		# AC_TRY_COMPILE with pthread.h, but that will work with
		# libpthread really doesn't exist, like AIX 4.2.
		# [Bug: 4359]
		AC_CHECK_LIB(pthread, __pthread_mutex_init,
		    tcl_ok=yes, tcl_ok=no)
	    fi
	    
	    if test "$tcl_ok" = "yes"; then
		# The space is needed
		THREADS_LIBS=" -lpthread"
	    else
		AC_CHECK_LIB(pthreads, pthread_mutex_init,
		    tcl_ok=yes, tcl_ok=no)
		if test "$tcl_ok" = "yes"; then
		    # The space is needed
		    THREADS_LIBS=" -lpthreads"
		else
		    AC_CHECK_LIB(c, pthread_mutex_init,
			tcl_ok=yes, tcl_ok=no)
		    if test "$tcl_ok" = "no"; then
			AC_CHECK_LIB(c_r, pthread_mutex_init,
			    tcl_ok=yes, tcl_ok=no)
			if test "$tcl_ok" = "yes"; then
			    # The space is needed
			    THREADS_LIBS=" -pthread"
			else
			    TCL_THREADS=0
			    AC_MSG_WARN("Don t know how to find pthread lib on your system - thread support disabled")
			fi
		    fi
		fi
	    fi

	    # Does the pthread-implementation provide
	    # 'pthread_attr_setstacksize' ?

	    ac_saved_libs=$LIBS
	    LIBS="$LIBS $THREADS_LIBS"
	    AC_CHECK_FUNCS(pthread_attr_setstacksize)
	    LIBS=$ac_saved_libs
	    AC_CHECK_FUNCS(readdir_r)
	fi
    else
	TCL_THREADS=0
    fi
    # Do checking message here to not mess up interleaved configure output
    AC_MSG_CHECKING([for building with threads])
    if test "${TCL_THREADS}" = "1"; then
	AC_DEFINE(TCL_THREADS)
	#LIBS="$LIBS $THREADS_LIBS"
	AC_MSG_RESULT([yes])
    else
	AC_MSG_RESULT([no (default)])
    fi
    # TCL_THREADS sanity checking.  See if our request for building with
    # threads is the same as the way Tcl was built.  If not, warn the user.
    case ${TCL_DEFS} in
	*THREADS=1*)
	    if test "${TCL_THREADS}" = "0"; then
		AC_MSG_WARN([
    Building ${PACKAGE_NAME} without threads enabled, but building against a Tcl
    that IS thread-enabled.])
	    fi
	    ;;
	*)
	    if test "${TCL_THREADS}" = "1"; then
		AC_MSG_WARN([
    --enable-threads requested, but attempting building against a Tcl
    that is NOT thread-enabled.])
	    fi
	    ;;
    esac
    AC_SUBST(TCL_THREADS)
])

#------------------------------------------------------------------------
# TEA_ENABLE_SYMBOLS --
#
#	Specify if debugging symbols should be used
#	Memory (TCL_MEM_DEBUG) debugging can also be enabled.
#
# Arguments:
#	none
#	
#	Requires the following vars to be set:
#		CFLAGS_DEBUG
#		CFLAGS_OPTIMIZE
#		LDFLAGS_DEBUG
#		LDFLAGS_OPTIMIZE
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-symbols
#
#	Defines the following vars:
#		CFLAGS_DEFAULT	Sets to CFLAGS_DEBUG if true
#				Sets to CFLAGS_OPTIMIZE if false
#		LDFLAGS_DEFAULT	Sets to LDFLAGS_DEBUG if true
#				Sets to LDFLAGS_OPTIMIZE if false
#		DBGX		Formerly used as debug library extension;
#				always blank now.
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_SYMBOLS, [
    dnl Make sure we are initialized
    AC_REQUIRE([TEA_CONFIG_CFLAGS])

    DBGX=""

    AC_MSG_CHECKING([for build with symbols])
    AC_ARG_ENABLE(symbols, [  --enable-symbols        build with debugging symbols [--disable-symbols]],    [tcl_ok=$enableval], [tcl_ok=no])
    if test "$tcl_ok" = "no"; then
	CFLAGS_DEFAULT="${CFLAGS_OPTIMIZE}"
	LDFLAGS_DEFAULT="${LDFLAGS_OPTIMIZE}"
	AC_MSG_RESULT([no])
    else
	CFLAGS_DEFAULT="${CFLAGS_DEBUG}"
	LDFLAGS_DEFAULT="${LDFLAGS_DEBUG}"
	if test "$tcl_ok" = "yes"; then
	    AC_MSG_RESULT([yes (standard debugging)])
	fi
    fi
    if test "${TEA_PLATFORM}" != "windows" ; then
	LDFLAGS_DEFAULT="${LDFLAGS}"
    fi

    AC_SUBST(TCL_DBGX)
    AC_SUBST(CFLAGS_DEFAULT)
    AC_SUBST(LDFLAGS_DEFAULT)

    if test "$tcl_ok" = "mem" -o "$tcl_ok" = "all"; then
	AC_DEFINE(TCL_MEM_DEBUG)
    fi

    if test "$tcl_ok" != "yes" -a "$tcl_ok" != "no"; then
	if test "$tcl_ok" = "all"; then
	    AC_MSG_RESULT([enabled symbols mem debugging])
	else
	    AC_MSG_RESULT([enabled $tcl_ok debugging])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_ENABLE_LANGINFO --
#
#	Allows use of modern nl_langinfo check for better l10n.
#	This is only relevant for Unix.
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-langinfo=yes|no (default is yes)
#
#	Defines the following vars:
#		HAVE_LANGINFO	Triggers use of nl_langinfo if defined.
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_LANGINFO, [
    AC_ARG_ENABLE(langinfo,
	[  --enable-langinfo	  use nl_langinfo if possible to determine
			  encoding at startup, otherwise use old heuristic],
	[langinfo_ok=$enableval], [langinfo_ok=yes])

    HAVE_LANGINFO=0
    if test "$langinfo_ok" = "yes"; then
	if test "$langinfo_ok" = "yes"; then
	    AC_CHECK_HEADER(langinfo.h,[langinfo_ok=yes],[langinfo_ok=no])
	fi
    fi
    AC_MSG_CHECKING([whether to use nl_langinfo])
    if test "$langinfo_ok" = "yes"; then
	AC_TRY_COMPILE([#include <langinfo.h>],
		[nl_langinfo(CODESET);],[langinfo_ok=yes],[langinfo_ok=no])
	if test "$langinfo_ok" = "no"; then
	    langinfo_ok="no (could not compile with nl_langinfo)";
	fi
	if test "$langinfo_ok" = "yes"; then
	    AC_DEFINE(HAVE_LANGINFO)
	fi
    fi
    AC_MSG_RESULT([$langinfo_ok])
])

#--------------------------------------------------------------------
# TEA_CONFIG_CFLAGS
#
#	Try to determine the proper flags to pass to the compiler
#	for building shared libraries and other such nonsense.
#
# Arguments:
#	none
#
# Results:
#
#	Defines the following vars:
#
#       DL_OBJS -       Name of the object file that implements dynamic
#                       loading for Tcl on this system.
#       DL_LIBS -       Library file(s) to include in tclsh and other base
#                       applications in order for the "load" command to work.
#       LDFLAGS -      Flags to pass to the compiler when linking object
#                       files into an executable application binary such
#                       as tclsh.
#       LD_SEARCH_FLAGS-Flags to pass to ld, such as "-R /usr/local/tcl/lib",
#                       that tell the run-time dynamic linker where to look
#                       for shared libraries such as libtcl.so.  Depends on
#                       the variable LIB_RUNTIME_DIR in the Makefile.
#       SHLIB_CFLAGS -  Flags to pass to cc when compiling the components
#                       of a shared library (may request position-independent
#                       code, among other things).
#       SHLIB_LD -      Base command to use for combining object files
#                       into a shared library.
#       SHLIB_LD_LIBS - Dependent libraries for the linker to scan when
#                       creating shared libraries.  This symbol typically
#                       goes at the end of the "ld" commands that build
#                       shared libraries. The value of the symbol is
#                       "${LIBS}" if all of the dependent libraries should
#                       be specified when creating a shared library.  If
#                       dependent libraries should not be specified (as on
#                       SunOS 4.x, where they cause the link to fail, or in
#                       general if Tcl and Tk aren't themselves shared
#                       libraries), then this symbol has an empty string
#                       as its value.
#       SHLIB_SUFFIX -  Suffix to use for the names of dynamically loadable
#                       extensions.  An empty string means we don't know how
#                       to use shared libraries on this platform.
#       TCL_LIB_FILE -  Name of the file that contains the Tcl library, such
#                       as libtcl7.8.so or libtcl7.8.a.
#       TCL_LIB_SUFFIX -Specifies everything that comes after the "libtcl"
#                       in the shared library name, using the
#                       ${PACKAGE_VERSION} variable to put the version in
#                       the right place.  This is used by platforms that
#                       need non-standard library names.
#                       Examples:  ${PACKAGE_VERSION}.so.1.1 on NetBSD,
#                       since it needs to have a version after the .so, and
#                       ${PACKAGE_VERSION}.a on AIX, since the Tcl shared
#                       library needs to have a .a extension whereas shared
#                       objects for loadable extensions have a .so
#                       extension.  Defaults to
#                       ${PACKAGE_VERSION}${SHLIB_SUFFIX}.
#       TCL_NEEDS_EXP_FILE -
#                       1 means that an export file is needed to link to a
#                       shared library.
#       TCL_EXP_FILE -  The name of the installed export / import file which
#                       should be used to link to the Tcl shared library.
#                       Empty if Tcl is unshared.
#       TCL_BUILD_EXP_FILE -
#                       The name of the built export / import file which
#                       should be used to link to the Tcl shared library.
#                       Empty if Tcl is unshared.
#	CFLAGS_DEBUG -
#			Flags used when running the compiler in debug mode
#	CFLAGS_OPTIMIZE -
#			Flags used when running the compiler in optimize mode
#	CFLAGS -	We add CFLAGS to pass to the compiler
#
#	Subst's the following vars:
#		DL_LIBS
#		CFLAGS_DEBUG
#		CFLAGS_OPTIMIZE
#		CFLAGS_WARNING
#
#		STLIB_LD
#		SHLIB_LD
#		SHLIB_CFLAGS
#		LDFLAGS_DEBUG
#		LDFLAGS_OPTIMIZE
#--------------------------------------------------------------------

AC_DEFUN(TEA_CONFIG_CFLAGS, [
    dnl Make sure we are initialized
    AC_REQUIRE([TEA_INIT])

    # Step 0: Enable 64 bit support?

    AC_MSG_CHECKING([if 64bit support is enabled])
    AC_ARG_ENABLE(64bit,[  --enable-64bit          enable 64bit support (where applicable)], [do64bit=$enableval], [do64bit=no])
    AC_MSG_RESULT([$do64bit])
 
    # Step 0.b: Enable Solaris 64 bit VIS support?

    AC_MSG_CHECKING([if 64bit Sparc VIS support is requested])
    AC_ARG_ENABLE(64bit-vis,[  --enable-64bit-vis      enable 64bit Sparc VIS support], [do64bitVIS=$enableval], [do64bitVIS=no])
    AC_MSG_RESULT([$do64bitVIS])

    if test "$do64bitVIS" = "yes"; then
	# Force 64bit on with VIS
	do64bit=yes
    fi

    # Step 0.c: Cross-compiling options for Windows/CE builds?

    if test "${TEA_PLATFORM}" = "windows" ; then
	AC_MSG_CHECKING([if Windows/CE build is requested])
	AC_ARG_ENABLE(wince,[  --enable-wince          enable Win/CE support (where applicable)], [doWince=$enableval], [doWince=no])
	AC_MSG_RESULT($doWince)
    fi

    # Step 1: set the variable "system" to hold the name and version number
    # for the system.  This can usually be done via the "uname" command, but
    # there are a few systems, like Next, where this doesn't work.

    AC_MSG_CHECKING([system version (for dynamic loading)])
    if test -f /usr/lib/NextStep/software_version; then
	system=NEXTSTEP-`awk '/3/,/3/' /usr/lib/NextStep/software_version`
    else
	system=`uname -s`-`uname -r`
	if test "$?" -ne 0 ; then
	    AC_MSG_RESULT([unknown (can't find uname command)])
	    system=unknown
	else
	    # Special check for weird MP-RAS system (uname returns weird
	    # results, and the version is kept in special file).
	
	    if test -r /etc/.relid -a "X`uname -n`" = "X`uname -s`" ; then
		system=MP-RAS-`awk '{print $3}' /etc/.relid'`
	    fi
	    if test "`uname -s`" = "AIX" ; then
		system=AIX-`uname -v`.`uname -r`
	    fi
	    if test "${TEA_PLATFORM}" = "windows" ; then
		system=windows
	    fi
	    AC_MSG_RESULT([$system])
	fi
    fi

    # Step 2: check for existence of -ldl library.  This is needed because
    # Linux can use either -ldl or -ldld for dynamic loading.

    AC_CHECK_LIB(dl, dlopen, have_dl=yes, have_dl=no)

    # Step 3: set configuration options based on system name and version.
    # This is similar to Tcl's unix/tcl.m4 except that we've added a
    # "windows" case and CC_SEARCH_FLAGS becomes LD_SEARCH_FLAGS for us
    # (and we have no CC_SEARCH_FLAGS).

    do64bit_ok=no
    LDFLAGS_ORIG="$LDFLAGS"
    TCL_EXPORT_FILE_SUFFIX=""
    UNSHARED_LIB_SUFFIX=""
    TCL_TRIM_DOTS='`echo ${PACKAGE_VERSION} | tr -d .`'
    ECHO_VERSION='`echo ${PACKAGE_VERSION}`'
    TCL_LIB_VERSIONS_OK=ok
    CFLAGS_DEBUG=-g
    if test "$GCC" = "yes" ; then
	CFLAGS_OPTIMIZE=-O2
	CFLAGS_WARNING="-Wall -Wno-implicit-int"
    else
	CFLAGS_OPTIMIZE=-O
	CFLAGS_WARNING=""
    fi
    TCL_NEEDS_EXP_FILE=0
    TCL_BUILD_EXP_FILE=""
    TCL_EXP_FILE=""
dnl FIXME: Replace AC_CHECK_PROG with AC_CHECK_TOOL once cross compiling is fixed.
dnl AC_CHECK_TOOL(AR, ar, :)
    AC_CHECK_PROG(AR, ar, ar)
    STLIB_LD='${AR} cr'
    LD_LIBRARY_PATH_VAR="LD_LIBRARY_PATH"
    case $system in
	windows)
	    # This is a 2-stage check to make sure we have the 64-bit SDK
	    # We have to know where the SDK is installed.
	    if test "$do64bit" = "yes" ; then
		if test "x${MSSDK}x" = "xx" ; then
		    MSSDK="C:/Progra~1/Microsoft SDK"
		fi
		# Ensure that this path has no spaces to work in autoconf
		TEA_PATH_NOSPACE(MSSDK, ${MSSDK})
		if test ! -d "${MSSDK}/bin/win64" ; then
		    AC_MSG_WARN([could not find 64-bit SDK to enable 64bit mode])
		    do64bit="no"
		else
		    do64bit_ok="yes"
		fi
	    fi

	    if test "$doWince" != "no" ; then
		if test "$do64bit" = "yes" ; then
		    AC_MSG_ERROR([Windows/CE and 64-bit builds incompatible])
		fi
		if test "$GCC" = "yes" ; then
		    AC_MSG_ERROR([Windows/CE and GCC builds incompatible])
		fi
		TEA_PATH_CELIB
		# Set defaults for common evc4/PPC2003 setup
		# Currently Tcl requires 300+, possibly 420+ for sockets
		CEVERSION=420; 		# could be 211 300 301 400 420 ...
		TARGETCPU=ARMV4;	# could be ARMV4 ARM MIPS SH3 X86 ...
		ARCH=ARM;		# could be ARM MIPS X86EM ...
		PLATFORM="Pocket PC 2003"; # or "Pocket PC 2002"
		if test "$doWince" != "yes"; then
		    # If !yes then the user specified something
		    # Reset ARCH to allow user to skip specifying it
		    ARCH=
		    eval `echo $doWince | awk -F, '{ \
	    if (length([$]1)) { printf "CEVERSION=\"%s\"\n", [$]1; \
	    if ([$]1 < 400)   { printf "PLATFORM=\"Pocket PC 2002\"\n" } }; \
	    if (length([$]2)) { printf "TARGETCPU=\"%s\"\n", toupper([$]2) }; \
	    if (length([$]3)) { printf "ARCH=\"%s\"\n", toupper([$]3) }; \
	    if (length([$]4)) { printf "PLATFORM=\"%s\"\n", [$]4 }; \
		    }'`
		    if test "x${ARCH}" = "x" ; then
			ARCH=$TARGETCPU;
		    fi
		fi
		OSVERSION=WCE$CEVERSION;
	    	if test "x${WCEROOT}" = "x" ; then
			WCEROOT="C:/Program Files/Microsoft eMbedded C++ 4.0"
		    if test ! -d "${WCEROOT}" ; then
			WCEROOT="C:/Program Files/Microsoft eMbedded Tools"
		    fi
		fi
		if test "x${SDKROOT}" = "x" ; then
		    SDKROOT="C:/Program Files/Windows CE Tools"
		    if test ! -d "${SDKROOT}" ; then
			SDKROOT="C:/Windows CE Tools"
		    fi
		fi
		# Ensure that this path has no spaces to work in autoconf
		TEA_PATH_NOSPACE(WCEROOT, ${WCEROOT})
		TEA_PATH_NOSPACE(SDKROOT, ${SDKROOT})
		if test ! -d "${SDKROOT}/${OSVERSION}/${PLATFORM}/Lib/${TARGETCPU}" \
		    -o ! -d "${WCEROOT}/EVC/${OSVERSION}/bin"; then
		    AC_MSG_ERROR([could not find PocketPC SDK or target compiler to enable WinCE mode [$CEVERSION,$TARGETCPU,$ARCH,$PLATFORM]])
		    doWince="no"
		else
		    # We could PATH_NOSPACE these, but that's not important,
		    # as long as we quote them when used.
		    CEINCLUDE="${SDKROOT}/${OSVERSION}/${PLATFORM}/include"
		    if test -d "${CEINCLUDE}/${TARGETCPU}" ; then
			CEINCLUDE="${CEINCLUDE}/${TARGETCPU}"
		    fi
		    CELIBPATH="${SDKROOT}/${OSVERSION}/${PLATFORM}/Lib/${TARGETCPU}"
    		fi
	    fi

	    if test "$GCC" != "yes" ; then
	        if test "${SHARED_BUILD}" = "0" ; then
		    runtime=-MT
	        else
		    runtime=-MD
	        fi

                if test "$do64bit" = "yes" ; then
		    # All this magic is necessary for the Win64 SDK RC1 - hobbs
		    CC="${MSSDK}/Bin/Win64/cl.exe"
		    CFLAGS="${CFLAGS} -I${MSSDK}/Include/prerelease \
			-I${MSSDK}/Include/Win64/crt \
			-I${MSSDK}/Include"
		    RC="${MSSDK}/bin/rc.exe"
		    lflags="-MACHINE:IA64 -LIBPATH:${MSSDK}/Lib/IA64 \
			-LIBPATH:${MSSDK}/Lib/Prerelease/IA64 -nologo"
		    LINKBIN="${MSSDK}/bin/win64/link.exe"
		    CFLAGS_DEBUG="-nologo -Zi -Od -W3 ${runtime}d"
		    CFLAGS_OPTIMIZE="-nologo -O2 -W2 ${runtime}"
		elif test "$doWince" != "no" ; then
		    CEBINROOT="${WCEROOT}/EVC/${OSVERSION}/bin"
		    if test "${TARGETCPU}" = "X86"; then
			CC="${CEBINROOT}/cl.exe"
		    else
			CC="${CEBINROOT}/cl${ARCH}.exe"
		    fi
		    CFLAGS="$CFLAGS -I\"${CELIB_DIR}/inc\" -I\"${CEINCLUDE}\""
		    RC="${WCEROOT}/Common/EVC/bin/rc.exe"
		    arch=`echo ${ARCH} | awk '{print tolower([$]0)}'`
		    defs="${ARCH} _${ARCH}_ ${arch} PALM_SIZE _MT _WINDOWS"
		    if test "${SHARED_BUILD}" = "1" ; then
			# Static CE builds require static celib as well
		    	defs="${defs} _DLL"
		    fi
		    for i in $defs ; do
			AC_DEFINE_UNQUOTED($i)
		    done
		    AC_DEFINE_UNQUOTED(_WIN32_WCE, $CEVERSION)
		    AC_DEFINE_UNQUOTED(UNDER_CE, $CEVERSION)
		    CFLAGS_DEBUG="-nologo -Zi -Od"
		    CFLAGS_OPTIMIZE="-nologo -Ox"
		    lversion=`echo ${CEVERSION} | sed -e 's/\(.\)\(..\)/\1\.\2/'`
		    lflags="-MACHINE:${ARCH} -LIBPATH:\"${CELIBPATH}\" -subsystem:windowsce,${lversion} -nologo"
		    LINKBIN="${CEBINROOT}/link.exe"
		    AC_SUBST(CELIB_DIR)
		else
		    RC="rc"
		    lflags="-nologo"
    		    LINKBIN="link"
		    CFLAGS_DEBUG="-nologo -Z7 -Od -W3 -WX ${runtime}d"
		    CFLAGS_OPTIMIZE="-nologo -O2 -W2 ${runtime}"
		fi
	    fi

	    if test "$GCC" = "yes"; then
		# mingw gcc mode
		RC="windres"
		CFLAGS_DEBUG="-g"
		CFLAGS_OPTIMIZE="-O2 -fomit-frame-pointer"
		SHLIB_LD="$CC -shared"
		UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
		LDFLAGS_CONSOLE="-wl,--subsystem,console ${lflags}"
		LDFLAGS_WINDOW="-wl,--subsystem,windows ${lflags}"
	    else
		SHLIB_LD="${LINKBIN} -dll ${lflags}"
		# link -lib only works when -lib is the first arg
		STLIB_LD="${LINKBIN} -lib ${lflags}"
		UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.lib'
		PATHTYPE=-w
		# For information on what debugtype is most useful, see:
		# http://msdn.microsoft.com/library/en-us/dnvc60/html/gendepdebug.asp
		# This essentially turns it all on.
		LDFLAGS_DEBUG="-debug:full -debugtype:both -warn:2"
		LDFLAGS_OPTIMIZE="-release"
		if test "$doWince" != "no" ; then
		    LDFLAGS_CONSOLE="-link ${lflags}"
		    LDFLAGS_WINDOW=${LDFLAGS_CONSOLE}
		else
		    LDFLAGS_CONSOLE="-link -subsystem:console ${lflags}"
		    LDFLAGS_WINDOW="-link -subsystem:windows ${lflags}"
		fi
	    fi

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".dll"
	    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.dll'

	    TCL_LIB_VERSIONS_OK=nodots
	    # Bogus to avoid getting this turned off
	    DL_OBJS="tclLoadNone.obj"
    	    ;;
	AIX-*)
	    if test "${TCL_THREADS}" = "1" -a "$GCC" != "yes" ; then
		# AIX requires the _r compiler when gcc isn't being used
		if test "${CC}" != "cc_r" ; then
		    CC=${CC}_r
		fi
		AC_MSG_RESULT([Using $CC for compiling with threads])
	    fi
	    LIBS="$LIBS -lc"
	    SHLIB_CFLAGS=""
	    SHLIB_SUFFIX=".so"
	    SHLIB_LD_LIBS='${LIBS}'

	    DL_OBJS="tclLoadDl.o"
	    LD_LIBRARY_PATH_VAR="LIBPATH"

	    # AIX v<=4.1 has some different flags than 4.2+
	    if test "$system" = "AIX-4.1" -o "`uname -v`" -lt "4" ; then
		#LIBOBJS="$LIBOBJS tclLoadAix.o"
		AC_LIBOBJ([tclLoadAix])
		DL_LIBS="-lld"
	    fi

	    # Check to enable 64-bit flags for compiler/linker on AIX 4+
	    if test "$do64bit" = "yes" -a "`uname -v`" -gt "3" ; then
		if test "$GCC" = "yes" ; then
		    AC_MSG_WARN("64bit mode not supported with GCC on $system")
		else 
		    do64bit_ok=yes
		    CFLAGS="$CFLAGS -q64"
		    LDFLAGS="$LDFLAGS -q64"
		    RANLIB="${RANLIB} -X64"
		    AR="${AR} -X64"
		    SHLIB_LD_FLAGS="-b64"
		fi
	    fi

	    if test "`uname -m`" = "ia64" ; then
		# AIX-5 uses ELF style dynamic libraries on IA-64, but not PPC
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		# AIX-5 has dl* in libc.so
		DL_LIBS=""
		if test "$GCC" = "yes" ; then
		    LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
		else
		    LD_SEARCH_FLAGS='-R${LIB_RUNTIME_DIR}'
		fi
	    else
		if test "$GCC" = "yes" ; then
		    SHLIB_LD="gcc -shared"
		else
		    SHLIB_LD="/bin/ld -bhalt:4 -bM:SRE -bE:lib.exp -H512 -T512 -bnoentry"
		fi
		SHLIB_LD="${TCL_SRC_DIR}/unix/ldAix ${SHLIB_LD} ${SHLIB_LD_FLAGS}"
		DL_LIBS="-ldl"
		LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
		TCL_NEEDS_EXP_FILE=1
		TCL_EXPORT_FILE_SUFFIX='${PACKAGE_VERSION}.exp'
	    fi

	    # On AIX <=v4 systems, libbsd.a has to be linked in to support
	    # non-blocking file IO.  This library has to be linked in after
	    # the MATH_LIBS or it breaks the pow() function.  The way to
	    # insure proper sequencing, is to add it to the tail of MATH_LIBS.
	    # This library also supplies gettimeofday.
	    #
	    # AIX does not have a timezone field in struct tm. When the AIX
	    # bsd library is used, the timezone global and the gettimeofday
	    # methods are to be avoided for timezone deduction instead, we
	    # deduce the timezone by comparing the localtime result on a
	    # known GMT value.

	    AC_CHECK_LIB(bsd, gettimeofday, libbsd=yes, libbsd=no)
	    if test $libbsd = yes; then
	    	MATH_LIBS="$MATH_LIBS -lbsd"
	    	AC_DEFINE(USE_DELTA_FOR_TZ)
	    fi
	    ;;
	BeOS*)
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD="${CC} -nostart"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    ;;
	BSD/OS-2.1*|BSD/OS-3*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="shlicc -r"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LD_SEARCH_FLAGS=""
	    ;;
	BSD/OS-4.*)
	    SHLIB_CFLAGS="-export-dynamic -fPIC"
	    SHLIB_LD="cc -shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS="$LDFLAGS -export-dynamic"
	    LD_SEARCH_FLAGS=""
	    ;;
	dgux*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LD_SEARCH_FLAGS=""
	    ;;
	HP-UX-*.11.*)
	    # Use updated header definitions where possible
	    AC_DEFINE(_XOPEN_SOURCE_EXTENDED)

	    SHLIB_SUFFIX=".sl"
	    AC_CHECK_LIB(dld, shl_load, tcl_ok=yes, tcl_ok=no)
	    if test "$tcl_ok" = yes; then
		SHLIB_CFLAGS="+z"
		SHLIB_LD="ld -b"
		SHLIB_LD_LIBS='${LIBS}'
		DL_OBJS="tclLoadShl.o"
		DL_LIBS="-ldld"
		LDFLAGS="$LDFLAGS -Wl,-E"
		LD_SEARCH_FLAGS='+s +b ${LIB_RUNTIME_DIR}:.'
		LD_LIBRARY_PATH_VAR="SHLIB_PATH"
	    fi
	    if test "$GCC" = "yes" ; then
		SHLIB_LD="gcc -shared"
		SHLIB_LD_LIBS='${LIBS}'
		LD_SEARCH_FLAGS='-Wl,+s,+b,${LIB_RUNTIME_DIR}:.'
	    fi

	    # Users may want PA-RISC 1.1/2.0 portable code - needs HP cc
	    #CFLAGS="$CFLAGS +DAportable"

	    # Check to enable 64-bit flags for compiler/linker
	    if test "$do64bit" = "yes" ; then
		if test "$GCC" = "yes" ; then
		    hpux_arch=`${CC} -dumpmachine`
		    case $hpux_arch in
			hppa64*)
			    # 64-bit gcc in use.  Fix flags for GNU ld.
			    do64bit_ok=yes
			    SHLIB_LD="${CC} -shared"
			    SHLIB_LD_LIBS='${LIBS}'
			    ;;
			*)
			    AC_MSG_WARN("64bit mode not supported with GCC on $system")
			    ;;
		    esac
		else
		    do64bit_ok=yes
		    CFLAGS="$CFLAGS +DD64"
		    LDFLAGS="$LDFLAGS +DD64"
		fi
	    fi
	    ;;
	HP-UX-*.08.*|HP-UX-*.09.*|HP-UX-*.10.*)
	    SHLIB_SUFFIX=".sl"
	    AC_CHECK_LIB(dld, shl_load, tcl_ok=yes, tcl_ok=no)
	    if test "$tcl_ok" = yes; then
		SHLIB_CFLAGS="+z"
		SHLIB_LD="ld -b"
		SHLIB_LD_LIBS=""
		DL_OBJS="tclLoadShl.o"
		DL_LIBS="-ldld"
		LDFLAGS="$LDFLAGS -Wl,-E"
		LD_SEARCH_FLAGS='-Wl,+s,+b,${LIB_RUNTIME_DIR}:.'
	    fi
	    LD_LIBRARY_PATH_VAR="SHLIB_PATH"
	    ;;
	IRIX-4.*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_SUFFIX=".a"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="$LDFLAGS -Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    SHARED_LIB_SUFFIX='${PACKAGE_VERSION}.a'
	    ;;
	IRIX-5.*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    ;;
	IRIX-6.*|IRIX64-6.5*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -n32 -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    if test "$GCC" = "yes" ; then
		CFLAGS="$CFLAGS -mabi=n32"
		LDFLAGS="$LDFLAGS -mabi=n32"
	    else
		case $system in
		    IRIX-6.3)
			# Use to build 6.2 compatible binaries on 6.3.
			CFLAGS="$CFLAGS -n32 -D_OLD_TERMIOS"
			;;
		    *)
			CFLAGS="$CFLAGS -n32"
			;;
		esac
		LDFLAGS="$LDFLAGS -n32"
	    fi
	    ;;
	IRIX64-6.*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -n32 -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'

	    # Check to enable 64-bit flags for compiler/linker

	    if test "$do64bit" = "yes" ; then
	        if test "$GCC" = "yes" ; then
	            AC_MSG_WARN([64bit mode not supported by gcc])
	        else
	            do64bit_ok=yes
	            SHLIB_LD="ld -64 -shared -rdata_shared"
	            CFLAGS="$CFLAGS -64"
	            LDFLAGS="$LDFLAGS -64"
	        fi
	    fi
	    ;;
	Linux*)
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"

	    CFLAGS_OPTIMIZE="-O2 -fomit-frame-pointer"
	    # egcs-2.91.66 on Redhat Linux 6.0 generates lots of warnings 
	    # when you inline the string and math operations.  Turn this off to
	    # get rid of the warnings.

	    #CFLAGS_OPTIMIZE="${CFLAGS_OPTIMIZE} -D__NO_STRING_INLINES -D__NO_MATH_INLINES"

	    if test "$have_dl" = yes; then
		SHLIB_LD="${CC} -shared"
		DL_OBJS="tclLoadDl.o"
		DL_LIBS="-ldl"
		LDFLAGS="$LDFLAGS -Wl,--export-dynamic"
		LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    else
		AC_CHECK_HEADER(dld.h, [
		    SHLIB_LD="ld -shared"
		    DL_OBJS="tclLoadDld.o"
		    DL_LIBS="-ldld"
		    LD_SEARCH_FLAGS=""])
	    fi
	    if test "`uname -m`" = "alpha" ; then
		CFLAGS="$CFLAGS -mieee"
	    fi

	    # The combo of gcc + glibc has a bug related
	    # to inlining of functions like strtod(). The
	    # -fno-builtin flag should address this problem
	    # but it does not work. The -fno-inline flag
	    # is kind of overkill but it works.
	    # Disable inlining only when one of the
	    # files in compat/*.c is being linked in.
	    if test x"${USE_COMPAT}" != x ; then
	        CFLAGS="$CFLAGS -fno-inline"
	    fi

	    ;;
	GNU*)
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"

	    if test "$have_dl" = yes; then
		SHLIB_LD="${CC} -shared"
		DL_OBJS=""
		DL_LIBS="-ldl"
		LDFLAGS="$LDFLAGS -Wl,--export-dynamic"
		LD_SEARCH_FLAGS=""
	    else
		AC_CHECK_HEADER(dld.h, [
		    SHLIB_LD="ld -shared"
		    DL_OBJS=""
		    DL_LIBS="-ldld"
		    LD_SEARCH_FLAGS=""])
	    fi
	    if test "`uname -m`" = "alpha" ; then
		CFLAGS="$CFLAGS -mieee"
	    fi
	    ;;
	MP-RAS-02*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LD_SEARCH_FLAGS=""
	    ;;
	MP-RAS-*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS="$LDFLAGS -Wl,-Bexport"
	    LD_SEARCH_FLAGS=""
	    ;;
	NetBSD-*|FreeBSD-[[1-2]].*)
	    # Not available on all versions:  check for include file.
	    AC_CHECK_HEADER(dlfcn.h, [
		# NetBSD/SPARC needs -fPIC, -fpic will not do.
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LD="ld -Bshareable -x"
		SHLIB_LD_LIBS=""
		SHLIB_SUFFIX=".so"
		DL_OBJS="tclLoadDl.o"
		DL_LIBS=""
		LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
		AC_MSG_CHECKING([for ELF])
		AC_EGREP_CPP(yes, [
#ifdef __ELF__
	yes
#endif
		],
		    AC_MSG_RESULT([yes])
		    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so',
		    AC_MSG_RESULT([no])
		    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so.1.0'
		)
	    ], [
		SHLIB_CFLAGS=""
		SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r"
		SHLIB_LD_LIBS='${LIBS}'
		SHLIB_SUFFIX=".a"
		DL_OBJS="tclLoadAout.o"
		DL_LIBS=""
		LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
		SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
	    ])

	    # FreeBSD doesn't handle version numbers with dots.

	    UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
	    TCL_LIB_VERSIONS_OK=nodots
	    ;;
	OpenBSD-*)
	    SHLIB_LD="${CC} -shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    AC_MSG_CHECKING(for ELF)
	    AC_EGREP_CPP(yes, [
#ifdef __ELF__
	yes
#endif
	    ],
		[AC_MSG_RESULT(yes)
		SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so.1.0'],
		[AC_MSG_RESULT(no)
		SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so.1.0']
	    )

	    # OpenBSD doesn't do version numbers with dots.
	    UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
	    TCL_LIB_VERSIONS_OK=nodots
	    ;;
	FreeBSD-*)
	    # FreeBSD 3.* and greater have ELF.
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD="ld -Bshareable -x"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LDFLAGS="$LDFLAGS -export-dynamic"
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    if test "${TCL_THREADS}" = "1" ; then
		# The -pthread needs to go in the CFLAGS, not LIBS
		LIBS=`echo $LIBS | sed s/-pthread//`
		CFLAGS="$CFLAGS -pthread"
	    	LDFLAGS="$LDFLAGS -pthread"
	    fi
	    case $system in
	    FreeBSD-3.*)
	    	# FreeBSD-3 doesn't handle version numbers with dots.
	    	UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
	    	SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so'
	    	TCL_LIB_VERSIONS_OK=nodots
		;;
	    esac
	    ;;
	Darwin-*)
	    SHLIB_CFLAGS="-fno-common"
	    SHLIB_LD="cc -dynamiclib \${LDFLAGS}"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".dylib"
	    DL_OBJS="tclLoadDyld.o"
	    DL_LIBS=""
	    LDFLAGS="$LDFLAGS -prebind -Wl,-search_paths_first"
	    LD_SEARCH_FLAGS=""
	    LD_LIBRARY_PATH_VAR="DYLD_LIBRARY_PATH"
	    CFLAGS_OPTIMIZE="-Os"
	    ;;
	NEXTSTEP-*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="cc -nostdlib -r"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadNext.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OS/390-*)
	    CFLAGS_OPTIMIZE=""      # Optimizer is buggy
	    AC_DEFINE(_OE_SOCKETS)  # needed in sys/socket.h
	    ;;      
	OSF1-1.0|OSF1-1.1|OSF1-1.2)
	    # OSF/1 1.[012] from OSF, and derivatives, including Paragon OSF/1
	    SHLIB_CFLAGS=""
	    # Hack: make package name same as library name
	    SHLIB_LD='ld -R -export $@:'
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadOSF.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OSF1-1.*)
	    # OSF/1 1.3 from OSF using ELF, and derivatives, including AD2
	    SHLIB_CFLAGS="-fPIC"
	    if test "$SHARED_BUILD" = "1" ; then
	        SHLIB_LD="ld -shared"
	    else
	        SHLIB_LD="ld -non_shared"
	    fi
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OSF1-V*)
	    # Digital OSF/1
	    SHLIB_CFLAGS=""
	    if test "$SHARED_BUILD" = "1" ; then
	        SHLIB_LD='ld -shared -expect_unresolved "*"'
	    else
	        SHLIB_LD='ld -non_shared -expect_unresolved "*"'
	    fi
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-rpath ${LIB_RUNTIME_DIR}'
	    if test "$GCC" = "yes" ; then
		CFLAGS="$CFLAGS -mieee"
            else
		CFLAGS="$CFLAGS -DHAVE_TZSET -std1 -ieee"
	    fi
	    # see pthread_intro(3) for pthread support on osf1, k.furukawa
	    if test "${TCL_THREADS}" = "1" ; then
		CFLAGS="$CFLAGS -DHAVE_PTHREAD_ATTR_SETSTACKSIZE"
		CFLAGS="$CFLAGS -DTCL_THREAD_STACK_MIN=PTHREAD_STACK_MIN*64"
		LIBS=`echo $LIBS | sed s/-lpthreads//`
		if test "$GCC" = "yes" ; then
		    LIBS="$LIBS -lpthread -lmach -lexc"
		else
		    CFLAGS="$CFLAGS -pthread"
		    LDFLAGS="$LDFLAGS -pthread"
		fi
	    fi

	    ;;
	QNX-6*)
	    # QNX RTP
	    # This may work for all QNX, but it was only reported for v6.
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD="ld -Bshareable -x"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    # dlopen is in -lc on QNX
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	RISCos-*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".a"
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="$LDFLAGS -Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    ;;
	SCO_SV-3.2*)
	    # Note, dlopen is available only on SCO 3.2.5 and greater. However,
	    # this test works, since "uname -s" was non-standard in 3.2.4 and
	    # below.
	    if test "$GCC" = "yes" ; then
	    	SHLIB_CFLAGS="-fPIC -melf"
	    	LDFLAGS="$LDFLAGS -melf -Wl,-Bexport"
	    else
	    	SHLIB_CFLAGS="-Kpic -belf"
	    	LDFLAGS="$LDFLAGS -belf -Wl,-Bexport"
	    fi
	    SHLIB_LD="ld -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	SINIX*5.4*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LD_SEARCH_FLAGS=""
	    ;;
	SunOS-4*)
	    SHLIB_CFLAGS="-PIC"
	    SHLIB_LD="ld"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'

	    # SunOS can't handle version numbers with dots in them in library
	    # specs, like -ltcl7.5, so use -ltcl75 instead.  Also, it
	    # requires an extra version number at the end of .so file names.
	    # So, the library has to have a name like libtcl75.so.1.0

	    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.so.1.0'
	    UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}.a'
	    TCL_LIB_VERSIONS_OK=nodots
	    ;;
	SunOS-5.[[0-6]]*)

	    # Note: If _REENTRANT isn't defined, then Solaris
	    # won't define thread-safe library routines.

	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_POSIX_PTHREAD_SEMANTICS)

	    SHLIB_CFLAGS="-KPIC"

	    # Note: need the LIBS below, otherwise Tk won't find Tcl's
	    # symbols when dynamically loaded into tclsh.

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    if test "$GCC" = "yes" ; then
		SHLIB_LD="$CC -shared"
		LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
	    else
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		LD_SEARCH_FLAGS='-R ${LIB_RUNTIME_DIR}'
	    fi
	    ;;
	SunOS-5*)

	    # Note: If _REENTRANT isn't defined, then Solaris
	    # won't define thread-safe library routines.

	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_POSIX_PTHREAD_SEMANTICS)

	    SHLIB_CFLAGS="-KPIC"
    
	    # Check to enable 64-bit flags for compiler/linker
	    if test "$do64bit" = "yes" ; then
		arch=`isainfo`
		if test "$arch" = "sparcv9 sparc" ; then
			if test "$GCC" = "yes" ; then
			    if test "`gcc -dumpversion` | awk -F. '{print $1}'" -lt "3" ; then
				AC_MSG_WARN([64bit mode not supported with GCC < 3.2 on $system])
			    else
				do64bit_ok=yes
				CFLAGS="$CFLAGS -m64 -mcpu=v9"
				LDFLAGS="$LDFLAGS -m64 -mcpu=v9"
				SHLIB_CFLAGS="-fPIC"
			    fi
			else
			    do64bit_ok=yes
			    if test "$do64bitVIS" = "yes" ; then
				CFLAGS="$CFLAGS -xarch=v9a"
			    	LDFLAGS="$LDFLAGS -xarch=v9a"
			    else
				CFLAGS="$CFLAGS -xarch=v9"
			    	LDFLAGS="$LDFLAGS -xarch=v9"
			    fi
			fi
		else
		    AC_MSG_WARN("64bit mode only supported sparcv9 system")
		fi
	    fi
	    
	    # Note: need the LIBS below, otherwise Tk won't find Tcl's
	    # symbols when dynamically loaded into tclsh.

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    if test "$GCC" = "yes" ; then
		SHLIB_LD="$CC -shared"
		LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
		if test "$do64bit" = "yes" ; then
		    # We need to specify -static-libgcc or we need to
		    # add the path to the sparv9 libgcc.
		    # JH: static-libgcc is necessary for core Tcl, but may
		    # not be necessary for extensions.
		    SHLIB_LD="$SHLIB_LD -m64 -mcpu=v9 -static-libgcc"
		    # for finding sparcv9 libgcc, get the regular libgcc
		    # path, remove so name and append 'sparcv9'
		    #v9gcclibdir="`gcc -print-file-name=libgcc_s.so` | ..."
		    #LD_SEARCH_FLAGS="${LD_SEARCH_FLAGS},-R,$v9gcclibdir"
		fi
	    else
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		LD_SEARCH_FLAGS='-R ${LIB_RUNTIME_DIR}'
	    fi
	    ;;
	ULTRIX-4.*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_SUFFIX=".a"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="$LDFLAGS -Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    if test "$GCC" != "yes" ; then
		CFLAGS="$CFLAGS -DHAVE_TZSET -std1"
	    fi
	    ;;
	UNIX_SV* | UnixWare-5*)
	    SHLIB_CFLAGS="-KPIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    # Some UNIX_SV* systems (unixware 1.1.2 for example) have linkers
	    # that don't grok the -Bexport option.  Test that it does.
	    hold_ldflags=$LDFLAGS
	    AC_MSG_CHECKING(for ld accepts -Bexport flag)
	    LDFLAGS="$LDFLAGS -Wl,-Bexport"
	    AC_TRY_LINK(, [int i;], [found=yes],
			[LDFLAGS=$hold_ldflags found=no])
	    AC_MSG_RESULT([$found])
	    LD_SEARCH_FLAGS=""
	    ;;
    esac

    if test "$do64bit" = "yes" -a "$do64bit_ok" = "no" ; then
    AC_MSG_WARN("64bit support being disabled -- don\'t know magic for this platform")
    fi

    # Step 4: If pseudo-static linking is in use (see K. B. Kenny, "Dynamic
    # Loading for Tcl -- What Became of It?".  Proc. 2nd Tcl/Tk Workshop,
    # New Orleans, LA, Computerized Processes Unlimited, 1994), then we need
    # to determine which of several header files defines the a.out file
    # format (a.out.h, sys/exec.h, or sys/exec_aout.h).  At present, we
    # support only a file format that is more or less version-7-compatible. 
    # In particular,
    #	- a.out files must begin with `struct exec'.
    #	- the N_TXTOFF on the `struct exec' must compute the seek address
    #	  of the text segment
    #	- The `struct exec' must contain a_magic, a_text, a_data, a_bss
    #	  and a_entry fields.
    # The following compilation should succeed if and only if either sys/exec.h
    # or a.out.h is usable for the purpose.
    #
    # Note that the modified COFF format used on MIPS Ultrix 4.x is usable; the
    # `struct exec' includes a second header that contains information that
    # duplicates the v7 fields that are needed.

    if test "x$DL_OBJS" = "xtclLoadAout.o" ; then
	AC_MSG_CHECKING([sys/exec.h])
	AC_TRY_COMPILE([#include <sys/exec.h>],[
	    struct exec foo;
	    unsigned long seek;
	    int flag;
#if defined(__mips) || defined(mips)
	    seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
	    seek = N_TXTOFF (foo);
#endif
	    flag = (foo.a_magic == OMAGIC);
	    return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
    ], tcl_ok=usable, tcl_ok=unusable)
	AC_MSG_RESULT([$tcl_ok])
	if test $tcl_ok = usable; then
	    AC_DEFINE(USE_SYS_EXEC_H)
	else
	    AC_MSG_CHECKING([a.out.h])
	    AC_TRY_COMPILE([#include <a.out.h>],[
		struct exec foo;
		unsigned long seek;
		int flag;
#if defined(__mips) || defined(mips)
		seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
		seek = N_TXTOFF (foo);
#endif
		flag = (foo.a_magic == OMAGIC);
		return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
	    ], tcl_ok=usable, tcl_ok=unusable)
	    AC_MSG_RESULT([$tcl_ok])
	    if test $tcl_ok = usable; then
		AC_DEFINE(USE_A_OUT_H)
	    else
		AC_MSG_CHECKING([sys/exec_aout.h])
		AC_TRY_COMPILE([#include <sys/exec_aout.h>],[
		    struct exec foo;
		    unsigned long seek;
		    int flag;
#if defined(__mips) || defined(mips)
		    seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
		    seek = N_TXTOFF (foo);
#endif
		    flag = (foo.a_midmag == OMAGIC);
		    return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
		], tcl_ok=usable, tcl_ok=unusable)
		AC_MSG_RESULT([$tcl_ok])
		if test $tcl_ok = usable; then
		    AC_DEFINE(USE_SYS_EXEC_AOUT_H)
		else
		    DL_OBJS=""
		fi
	    fi
	fi
    fi

    # Step 5: disable dynamic loading if requested via a command-line switch.

    AC_ARG_ENABLE(load, [  --disable-load          disallow dynamic loading and "load" command],
	[tcl_ok=$enableval], [tcl_ok=yes])
    if test "$tcl_ok" = "no"; then
	DL_OBJS=""
    fi

    if test "x$DL_OBJS" != "x" ; then
	BUILD_DLTEST="\$(DLTEST_TARGETS)"
    else
	echo "Can't figure out how to do dynamic loading or shared libraries"
	echo "on this system."
	SHLIB_CFLAGS=""
	SHLIB_LD=""
	SHLIB_SUFFIX=""
	DL_OBJS="tclLoadNone.o"
	DL_LIBS=""
	LDFLAGS="$LDFLAGS_ORIG"
	LD_SEARCH_FLAGS=""
	BUILD_DLTEST=""
    fi

    # If we're running gcc, then change the C flags for compiling shared
    # libraries to the right flags for gcc, instead of those for the
    # standard manufacturer compiler.

    if test "$DL_OBJS" != "tclLoadNone.o" ; then
	if test "$GCC" = "yes" ; then
	    case $system in
		AIX-*)
		    ;;
		BSD/OS*)
		    ;;
		IRIX*)
		    ;;
		NetBSD-*|FreeBSD-*)
		    ;;
		Darwin-*)
		    ;;
		RISCos-*)
		    ;;
		SCO_SV-3.2*)
		    ;;
		ULTRIX-4.*)
		    ;;
		windows)
		    ;;
		*)
		    SHLIB_CFLAGS="-fPIC"
		    ;;
	    esac
	fi
    fi

    if test "$SHARED_LIB_SUFFIX" = "" ; then
	SHARED_LIB_SUFFIX='${PACKAGE_VERSION}${SHLIB_SUFFIX}'
    fi
    if test "$UNSHARED_LIB_SUFFIX" = "" ; then
	UNSHARED_LIB_SUFFIX='${PACKAGE_VERSION}.a'
    fi

    AC_SUBST(DL_LIBS)
    AC_SUBST(CFLAGS_DEBUG)
    AC_SUBST(CFLAGS_OPTIMIZE)
    AC_SUBST(CFLAGS_WARNING)

    AC_SUBST(STLIB_LD)
    AC_SUBST(SHLIB_LD)
    AC_SUBST(SHLIB_CFLAGS)
    AC_SUBST(SHLIB_LD_LIBS)
    AC_SUBST(LDFLAGS_DEBUG)
    AC_SUBST(LDFLAGS_OPTIMIZE)
    AC_SUBST(LD_LIBRARY_PATH_VAR)

    # These must be called after we do the basic CFLAGS checks and
    # verify any possible 64-bit or similar switches are necessary
    TEA_TCL_EARLY_FLAGS
    TEA_TCL_64BIT_FLAGS
])

#--------------------------------------------------------------------
# TEA_SERIAL_PORT
#
#	Determine which interface to use to talk to the serial port.
#	Note that #include lines must begin in leftmost column for
#	some compilers to recognize them as preprocessor directives,
#	and some build environments have stdin not pointing at a
#	pseudo-terminal (usually /dev/null instead.)
#
# Arguments:
#	none
#	
# Results:
#
#	Defines only one of the following vars:
#		HAVE_SYS_MODEM_H
#		USE_TERMIOS
#		USE_TERMIO
#		USE_SGTTY
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_SERIAL_PORT, [
    AC_CHECK_HEADERS(sys/modem.h)
    AC_MSG_CHECKING([termios vs. termio vs. sgtty])
    AC_CACHE_VAL(tcl_cv_api_serial, [
    AC_TRY_RUN([
#include <termios.h>

int main() {
    struct termios t;
    if (tcgetattr(0, &t) == 0) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termios, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <termio.h>

int main() {
    struct termio t;
    if (ioctl(0, TCGETA, &t) == 0) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termio, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <sgtty.h>

int main() {
    struct sgttyb t;
    if (ioctl(0, TIOCGETP, &t) == 0) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=sgtty, tcl_cv_api_serial=none, tcl_cv_api_serial=none)
    fi
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <termios.h>
#include <errno.h>

int main() {
    struct termios t;
    if (tcgetattr(0, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termios, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no; then
	AC_TRY_RUN([
#include <termio.h>
#include <errno.h>

int main() {
    struct termio t;
    if (ioctl(0, TCGETA, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
    }], tcl_cv_api_serial=termio, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no; then
	AC_TRY_RUN([
#include <sgtty.h>
#include <errno.h>

int main() {
    struct sgttyb t;
    if (ioctl(0, TIOCGETP, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=sgtty, tcl_cv_api_serial=none, tcl_cv_api_serial=none)
    fi])
    case $tcl_cv_api_serial in
	termios) AC_DEFINE(USE_TERMIOS);;
	termio)  AC_DEFINE(USE_TERMIO);;
	sgtty)   AC_DEFINE(USE_SGTTY);;
    esac
    AC_MSG_RESULT([$tcl_cv_api_serial])
])

#--------------------------------------------------------------------
# TEA_MISSING_POSIX_HEADERS
#
#	Supply substitutes for missing POSIX header files.  Special
#	notes:
#	    - stdlib.h doesn't define strtol, strtoul, or
#	      strtod insome versions of SunOS
#	    - some versions of string.h don't declare procedures such
#	      as strstr
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		NO_DIRENT_H
#		NO_ERRNO_H
#		NO_VALUES_H
#		HAVE_LIMITS_H or NO_LIMITS_H
#		NO_STDLIB_H
#		NO_STRING_H
#		NO_SYS_WAIT_H
#		NO_DLFCN_H
#		HAVE_SYS_PARAM_H
#
#		HAVE_STRING_H ?
#
# tkUnixPort.h checks for HAVE_LIMITS_H, so do both HAVE and
# CHECK on limits.h
#--------------------------------------------------------------------

AC_DEFUN(TEA_MISSING_POSIX_HEADERS, [
    AC_MSG_CHECKING([dirent.h])
    AC_TRY_LINK([#include <sys/types.h>
#include <dirent.h>], [
#ifndef _POSIX_SOURCE
#   ifdef __Lynx__
	/*
	 * Generate compilation error to make the test fail:  Lynx headers
	 * are only valid if really in the POSIX environment.
	 */

	missing_procedure();
#   endif
#endif
DIR *d;
struct dirent *entryPtr;
char *p;
d = opendir("foobar");
entryPtr = readdir(d);
p = entryPtr->d_name;
closedir(d);
], tcl_ok=yes, tcl_ok=no)

    if test $tcl_ok = no; then
	AC_DEFINE(NO_DIRENT_H)
    fi

    AC_MSG_RESULT([$tcl_ok])
    AC_CHECK_HEADER(errno.h, , [AC_DEFINE(NO_ERRNO_H)])
    AC_CHECK_HEADER(float.h, , [AC_DEFINE(NO_FLOAT_H)])
    AC_CHECK_HEADER(values.h, , [AC_DEFINE(NO_VALUES_H)])
    AC_CHECK_HEADER(limits.h,
	[AC_DEFINE(HAVE_LIMITS_H)], [AC_DEFINE(NO_LIMITS_H)])
    AC_CHECK_HEADER(stdlib.h, tcl_ok=1, tcl_ok=0)
    AC_EGREP_HEADER(strtol, stdlib.h, , tcl_ok=0)
    AC_EGREP_HEADER(strtoul, stdlib.h, , tcl_ok=0)
    AC_EGREP_HEADER(strtod, stdlib.h, , tcl_ok=0)
    if test $tcl_ok = 0; then
	AC_DEFINE(NO_STDLIB_H)
    fi
    AC_CHECK_HEADER(string.h, tcl_ok=1, tcl_ok=0)
    AC_EGREP_HEADER(strstr, string.h, , tcl_ok=0)
    AC_EGREP_HEADER(strerror, string.h, , tcl_ok=0)

    # See also memmove check below for a place where NO_STRING_H can be
    # set and why.

    if test $tcl_ok = 0; then
	AC_DEFINE(NO_STRING_H)
    fi

    AC_CHECK_HEADER(sys/wait.h, , [AC_DEFINE(NO_SYS_WAIT_H)])
    AC_CHECK_HEADER(dlfcn.h, , [AC_DEFINE(NO_DLFCN_H)])

    # OS/390 lacks sys/param.h (and doesn't need it, by chance).
    AC_HAVE_HEADERS(sys/param.h)

])

#--------------------------------------------------------------------
# TEA_PATH_X
#
#	Locate the X11 header files and the X11 library archive.  Try
#	the ac_path_x macro first, but if it doesn't find the X stuff
#	(e.g. because there's no xmkmf program) then check through
#	a list of possible directories.  Under some conditions the
#	autoconf macro will return an include directory that contains
#	no include files, so double-check its result just to be safe.
#
#	This should be called after TEA_CONFIG_CFLAGS as setting the
#	LIBS line can confuse some configure macro magic.
#
# Arguments:
#	none
#	
# Results:
#
#	Sets the following vars:
#		XINCLUDES
#		XLIBSW
#		LIBS (appends to)
#		TEA_WINDOWINGSYSTEM
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_PATH_X, [
    if test "${TEA_PLATFORM}" = "unix" ; then
	case ${TK_DEFS} in
	    *MAC_OSX_TK*)
		AC_DEFINE(MAC_OSX_TK)
		TEA_WINDOWINGSYSTEM="aqua"
		;;
	    *)
		TEA_PATH_UNIX_X
		TEA_WINDOWINGSYSTEM="x11"
		;;
	esac
    elif test "${TEA_PLATFORM}" = "windows" ; then
	TEA_WINDOWINGSYSTEM="windows"
    fi
])

AC_DEFUN(TEA_PATH_UNIX_X, [
    AC_PATH_X
    not_really_there=""
    if test "$no_x" = ""; then
	if test "$x_includes" = ""; then
	    AC_TRY_CPP([#include <X11/XIntrinsic.h>], , not_really_there="yes")
	else
	    if test ! -r $x_includes/X11/Intrinsic.h; then
		not_really_there="yes"
	    fi
	fi
    fi
    if test "$no_x" = "yes" -o "$not_really_there" = "yes"; then
	AC_MSG_CHECKING([for X11 header files])
	XINCLUDES="# no special path needed"
	AC_TRY_CPP([#include <X11/Intrinsic.h>], , XINCLUDES="nope")
	if test "$XINCLUDES" = nope; then
	    dirs="/usr/unsupported/include /usr/local/include /usr/X386/include /usr/X11R6/include /usr/X11R5/include /usr/include/X11R5 /usr/include/X11R4 /usr/openwin/include /usr/X11/include /usr/sww/include"
	    for i in $dirs ; do
		if test -r $i/X11/Intrinsic.h; then
		    AC_MSG_RESULT([$i])
		    XINCLUDES=" -I$i"
		    break
		fi
	    done
	fi
    else
	if test "$x_includes" != ""; then
	    XINCLUDES=-I$x_includes
	else
	    XINCLUDES="# no special path needed"
	fi
    fi
    if test "$XINCLUDES" = nope; then
	AC_MSG_RESULT([could not find any!])
	XINCLUDES="# no include files found"
    fi

    if test "$no_x" = yes; then
	AC_MSG_CHECKING([for X11 libraries])
	XLIBSW=nope
	dirs="/usr/unsupported/lib /usr/local/lib /usr/X386/lib /usr/X11R6/lib /usr/X11R5/lib /usr/lib/X11R5 /usr/lib/X11R4 /usr/openwin/lib /usr/X11/lib /usr/sww/X11/lib"
	for i in $dirs ; do
	    if test -r $i/libX11.a -o -r $i/libX11.so -o -r $i/libX11.sl; then
		AC_MSG_RESULT([$i])
		XLIBSW="-L$i -lX11"
		x_libraries="$i"
		break
	    fi
	done
    else
	if test "$x_libraries" = ""; then
	    XLIBSW=-lX11
	else
	    XLIBSW="-L$x_libraries -lX11"
	fi
    fi
    if test "$XLIBSW" = nope ; then
	AC_CHECK_LIB(Xwindow, XCreateWindow, XLIBSW=-lXwindow)
    fi
    if test "$XLIBSW" = nope ; then
	AC_MSG_RESULT([could not find any!  Using -lX11.])
	XLIBSW=-lX11
    fi
    if test x"${XLIBSW}" != x ; then
	PKG_LIBS="${PKG_LIBS} ${XLIBSW}"
    fi
])

#--------------------------------------------------------------------
# TEA_BLOCKING_STYLE
#
#	The statements below check for systems where POSIX-style
#	non-blocking I/O (O_NONBLOCK) doesn't work or is unimplemented. 
#	On these systems (mostly older ones), use the old BSD-style
#	FIONBIO approach instead.
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		HAVE_SYS_IOCTL_H
#		HAVE_SYS_FILIO_H
#		USE_FIONBIO
#		O_NONBLOCK
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_BLOCKING_STYLE, [
    AC_CHECK_HEADERS(sys/ioctl.h)
    AC_CHECK_HEADERS(sys/filio.h)
    AC_MSG_CHECKING([FIONBIO vs. O_NONBLOCK for nonblocking I/O])
    if test -f /usr/lib/NextStep/software_version; then
	system=NEXTSTEP-`awk '/3/,/3/' /usr/lib/NextStep/software_version`
    else
	system=`uname -s`-`uname -r`
	if test "$?" -ne 0 ; then
	    system=unknown
	else
	    # Special check for weird MP-RAS system (uname returns weird
	    # results, and the version is kept in special file).
	
	    if test -r /etc/.relid -a "X`uname -n`" = "X`uname -s`" ; then
		system=MP-RAS-`awk '{print $3}' /etc/.relid'`
	    fi
	    if test "`uname -s`" = "AIX" ; then
		system=AIX-`uname -v`.`uname -r`
	    fi
	fi
    fi
    case $system in
	# There used to be code here to use FIONBIO under AIX.  However, it
	# was reported that FIONBIO doesn't work under AIX 3.2.5.  Since
	# using O_NONBLOCK seems fine under AIX 4.*, I removed the FIONBIO
	# code (JO, 5/31/97).

	OSF*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	SunOS-4*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	ULTRIX-4.*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	*)
	    AC_MSG_RESULT([O_NONBLOCK])
	    ;;
    esac
])

#--------------------------------------------------------------------
# TEA_TIME_HANLDER
#
#	Checks how the system deals with time.h, what time structures
#	are used on the system, and what fields the structures have.
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		USE_DELTA_FOR_TZ
#		HAVE_TM_GMTOFF
#		HAVE_TM_TZADJ
#		HAVE_TIMEZONE_VAR
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TIME_HANDLER, [
    AC_CHECK_HEADERS(sys/time.h)
    AC_HEADER_TIME
    AC_STRUCT_TIMEZONE

    AC_CHECK_FUNCS(gmtime_r localtime_r)

    AC_MSG_CHECKING([tm_tzadj in struct tm])
    AC_CACHE_VAL(tcl_cv_member_tm_tzadj,
	AC_TRY_COMPILE([#include <time.h>], [struct tm tm; tm.tm_tzadj;],
	    tcl_cv_member_tm_tzadj=yes, tcl_cv_member_tm_tzadj=no))
    AC_MSG_RESULT([$tcl_cv_member_tm_tzadj])
    if test $tcl_cv_member_tm_tzadj = yes ; then
	AC_DEFINE(HAVE_TM_TZADJ)
    fi

    AC_MSG_CHECKING([tm_gmtoff in struct tm])
    AC_CACHE_VAL(tcl_cv_member_tm_gmtoff,
	AC_TRY_COMPILE([#include <time.h>], [struct tm tm; tm.tm_gmtoff;],
	    tcl_cv_member_tm_gmtoff=yes, tcl_cv_member_tm_gmtoff=no))
    AC_MSG_RESULT([$tcl_cv_member_tm_gmtoff])
    if test $tcl_cv_member_tm_gmtoff = yes ; then
	AC_DEFINE(HAVE_TM_GMTOFF)
    fi

    #
    # Its important to include time.h in this check, as some systems
    # (like convex) have timezone functions, etc.
    #
    AC_MSG_CHECKING([long timezone variable])
    AC_CACHE_VAL(tcl_cv_var_timezone,
	AC_TRY_COMPILE([#include <time.h>],
	    [extern long timezone;
	    timezone += 1;
	    exit (0);],
	    tcl_cv_timezone_long=yes, tcl_cv_timezone_long=no))
    AC_MSG_RESULT([$tcl_cv_timezone_long])
    if test $tcl_cv_timezone_long = yes ; then
	AC_DEFINE(HAVE_TIMEZONE_VAR)
    else
	#
	# On some systems (eg IRIX 6.2), timezone is a time_t and not a long.
	#
	AC_MSG_CHECKING([time_t timezone variable])
	AC_CACHE_VAL(tcl_cv_timezone_time,
	    AC_TRY_COMPILE([#include <time.h>],
		[extern time_t timezone;
		timezone += 1;
		exit (0);],
		tcl_cv_timezone_time=yes, tcl_cv_timezone_time=no))
	AC_MSG_RESULT([$tcl_cv_timezone_time])
	if test $tcl_cv_timezone_time = yes ; then
	    AC_DEFINE(HAVE_TIMEZONE_VAR)
	fi
    fi
])

#--------------------------------------------------------------------
# TEA_BUGGY_STRTOD
#
#	Under Solaris 2.4, strtod returns the wrong value for the
#	terminating character under some conditions.  Check for this
#	and if the problem exists use a substitute procedure
#	"fixstrtod" (provided by Tcl) that corrects the error.
#	Also, on Compaq's Tru64 Unix 5.0,
#	strtod(" ") returns 0.0 instead of a failure to convert.
#
# Arguments:
#	none
#	
# Results:
#
#	Might defines some of the following vars:
#		strtod (=fixstrtod)
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_BUGGY_STRTOD, [
    AC_CHECK_FUNC(strtod, tcl_strtod=1, tcl_strtod=0)
    if test "$tcl_strtod" = 1; then
	AC_MSG_CHECKING([for Solaris2.4/Tru64 strtod bugs])
	AC_TRY_RUN([
	    extern double strtod();
	    int main()
	    {
		char *string = "NaN", *spaceString = " ";
		char *term;
		double value;
		value = strtod(string, &term);
		if ((term != string) && (term[-1] == 0)) {
		    exit(1);
		}
		value = strtod(spaceString, &term);
		if (term == (spaceString+1)) {
		    exit(1);
		}
		exit(0);
	    }], tcl_ok=1, tcl_ok=0, tcl_ok=0)
	if test "$tcl_ok" = 1; then
	    AC_MSG_RESULT([ok])
	else
	    AC_MSG_RESULT([buggy])
	    #LIBOBJS="$LIBOBJS fixstrtod.o"
	    AC_LIBOBJ([fixstrtod])
	    USE_COMPAT=1
	    AC_DEFINE(strtod, fixstrtod)
	fi
    fi
])

#--------------------------------------------------------------------
# TEA_TCL_LINK_LIBS
#
#	Search for the libraries needed to link the Tcl shell.
#	Things like the math library (-lm) and socket stuff (-lsocket vs.
#	-lnsl) are dealt with here.
#
# Arguments:
#	Requires the following vars to be set in the Makefile:
#		DL_LIBS
#		LIBS
#		MATH_LIBS
#	
# Results:
#
#	Subst's the following var:
#		TCL_LIBS
#		MATH_LIBS
#
#	Might append to the following vars:
#		LIBS
#
#	Might define the following vars:
#		HAVE_NET_ERRNO_H
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_LINK_LIBS, [
    #--------------------------------------------------------------------
    # On a few very rare systems, all of the libm.a stuff is
    # already in libc.a.  Set compiler flags accordingly.
    # Also, Linux requires the "ieee" library for math to work
    # right (and it must appear before "-lm").
    #--------------------------------------------------------------------

    AC_CHECK_FUNC(sin, MATH_LIBS="", MATH_LIBS="-lm")
    AC_CHECK_LIB(ieee, main, [MATH_LIBS="-lieee $MATH_LIBS"])

    #--------------------------------------------------------------------
    # Interactive UNIX requires -linet instead of -lsocket, plus it
    # needs net/errno.h to define the socket-related error codes.
    #--------------------------------------------------------------------

    AC_CHECK_LIB(inet, main, [LIBS="$LIBS -linet"])
    AC_CHECK_HEADER(net/errno.h, AC_DEFINE(HAVE_NET_ERRNO_H))

    #--------------------------------------------------------------------
    #	Check for the existence of the -lsocket and -lnsl libraries.
    #	The order here is important, so that they end up in the right
    #	order in the command line generated by make.  Here are some
    #	special considerations:
    #	1. Use "connect" and "accept" to check for -lsocket, and
    #	   "gethostbyname" to check for -lnsl.
    #	2. Use each function name only once:  can't redo a check because
    #	   autoconf caches the results of the last check and won't redo it.
    #	3. Use -lnsl and -lsocket only if they supply procedures that
    #	   aren't already present in the normal libraries.  This is because
    #	   IRIX 5.2 has libraries, but they aren't needed and they're
    #	   bogus:  they goof up name resolution if used.
    #	4. On some SVR4 systems, can't use -lsocket without -lnsl too.
    #	   To get around this problem, check for both libraries together
    #	   if -lsocket doesn't work by itself.
    #--------------------------------------------------------------------

    tcl_checkBoth=0
    AC_CHECK_FUNC(connect, tcl_checkSocket=0, tcl_checkSocket=1)
    if test "$tcl_checkSocket" = 1; then
	AC_CHECK_FUNC(setsockopt, , [AC_CHECK_LIB(socket, setsockopt,
	    LIBS="$LIBS -lsocket", tcl_checkBoth=1)])
    fi
    if test "$tcl_checkBoth" = 1; then
	tk_oldLibs=$LIBS
	LIBS="$LIBS -lsocket -lnsl"
	AC_CHECK_FUNC(accept, tcl_checkNsl=0, [LIBS=$tk_oldLibs])
    fi
    AC_CHECK_FUNC(gethostbyname, , [AC_CHECK_LIB(nsl, gethostbyname,
	    [LIBS="$LIBS -lnsl"])])
    
    # Don't perform the eval of the libraries here because DL_LIBS
    # won't be set until we call TEA_CONFIG_CFLAGS

    TCL_LIBS='${DL_LIBS} ${LIBS} ${MATH_LIBS}'
    AC_SUBST(TCL_LIBS)
    AC_SUBST(MATH_LIBS)
])

#--------------------------------------------------------------------
# TEA_TCL_EARLY_FLAGS
#
#	Check for what flags are needed to be passed so the correct OS
#	features are available.
#
# Arguments:
#	None
#	
# Results:
#
#	Might define the following vars:
#		_ISOC99_SOURCE
#		_LARGEFILE64_SOURCE
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_EARLY_FLAG,[
    AC_CACHE_VAL([tcl_cv_flag_]translit($1,[A-Z],[a-z]),
	AC_TRY_COMPILE([$2], $3, [tcl_cv_flag_]translit($1,[A-Z],[a-z])=no,
	    AC_TRY_COMPILE([[#define ]$1[ 1
]$2], $3,
		[tcl_cv_flag_]translit($1,[A-Z],[a-z])=yes,
		[tcl_cv_flag_]translit($1,[A-Z],[a-z])=no)))
    if test ["x${tcl_cv_flag_]translit($1,[A-Z],[a-z])[}" = "xyes"] ; then
	AC_DEFINE($1)
	tcl_flags="$tcl_flags $1"
    fi
])

AC_DEFUN(TEA_TCL_EARLY_FLAGS,[
    AC_MSG_CHECKING([for required early compiler flags])
    tcl_flags=""
    TEA_TCL_EARLY_FLAG(_ISOC99_SOURCE,[#include <stdlib.h>],
	[char *p = (char *)strtoll; char *q = (char *)strtoull;])
    TEA_TCL_EARLY_FLAG(_LARGEFILE64_SOURCE,[#include <sys/stat.h>],
	[struct stat64 buf; int i = stat64("/", &buf);])
    if test "x${tcl_flags}" = "x" ; then
	AC_MSG_RESULT([none])
    else
	AC_MSG_RESULT([${tcl_flags}])
    fi
])

#--------------------------------------------------------------------
# TEA_TCL_64BIT_FLAGS
#
#	Check for what is defined in the way of 64-bit features.
#
# Arguments:
#	None
#	
# Results:
#
#	Might define the following vars:
#		TCL_WIDE_INT_IS_LONG
#		TCL_WIDE_INT_TYPE
#		HAVE_STRUCT_DIRENT64
#		HAVE_STRUCT_STAT64
#		HAVE_TYPE_OFF64_T
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_64BIT_FLAGS, [
    AC_MSG_CHECKING([for 64-bit integer type])
    AC_CACHE_VAL(tcl_cv_type_64bit,[
	tcl_cv_type_64bit=none
	# See if the compiler knows natively about __int64
	AC_TRY_COMPILE(,[__int64 value = (__int64) 0;],
	    tcl_type_64bit=__int64, tcl_type_64bit="long long")
	# See if we should use long anyway  Note that we substitute in the
	# type that is our current guess for a 64-bit type inside this check
	# program, so it should be modified only carefully...
        AC_TRY_COMPILE(,[switch (0) { 
            case 1: case (sizeof(]${tcl_type_64bit}[)==sizeof(long)): ; 
        }],tcl_cv_type_64bit=${tcl_type_64bit})])
    if test "${tcl_cv_type_64bit}" = none ; then
	AC_DEFINE(TCL_WIDE_INT_IS_LONG)
	AC_MSG_RESULT([using long])
    elif test "${tcl_cv_type_64bit}" = "__int64" \
		-a "${TEA_PLATFORM}" = "windows" ; then
	# We actually want to use the default tcl.h checks in this
	# case to handle both TCL_WIDE_INT_TYPE and TCL_LL_MODIFIER*
	AC_MSG_RESULT([using Tcl header defaults])
    else
	AC_DEFINE_UNQUOTED(TCL_WIDE_INT_TYPE,${tcl_cv_type_64bit})
	AC_MSG_RESULT([${tcl_cv_type_64bit}])

	# Now check for auxiliary declarations
	AC_MSG_CHECKING([for struct dirent64])
	AC_CACHE_VAL(tcl_cv_struct_dirent64,[
	    AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/dirent.h>],[struct dirent64 p;],
		tcl_cv_struct_dirent64=yes,tcl_cv_struct_dirent64=no)])
	if test "x${tcl_cv_struct_dirent64}" = "xyes" ; then
	    AC_DEFINE(HAVE_STRUCT_DIRENT64)
	fi
	AC_MSG_RESULT([${tcl_cv_struct_dirent64}])

	AC_MSG_CHECKING([for struct stat64])
	AC_CACHE_VAL(tcl_cv_struct_stat64,[
	    AC_TRY_COMPILE([#include <sys/stat.h>],[struct stat64 p;
],
		tcl_cv_struct_stat64=yes,tcl_cv_struct_stat64=no)])
	if test "x${tcl_cv_struct_stat64}" = "xyes" ; then
	    AC_DEFINE(HAVE_STRUCT_STAT64)
	fi
	AC_MSG_RESULT([${tcl_cv_struct_stat64}])

	AC_MSG_CHECKING([for off64_t])
	AC_CACHE_VAL(tcl_cv_type_off64_t,[
	    AC_TRY_COMPILE([#include <sys/types.h>],[off64_t offset;
],
		tcl_cv_type_off64_t=yes,tcl_cv_type_off64_t=no)])
	if test "x${tcl_cv_type_off64_t}" = "xyes" ; then
	    AC_DEFINE(HAVE_TYPE_OFF64_T)
	fi
	AC_MSG_RESULT([${tcl_cv_type_off64_t}])
    fi
])

##
## Here ends the standard Tcl configuration bits and starts the
## TEA specific functions
##

#------------------------------------------------------------------------
# TEA_INIT --
#
#	Init various Tcl Extension Architecture (TEA) variables.
#	This should be the first called TEA_* macro.
#
# Arguments:
#	none
#
# Results:
#
#	Defines and substs the following vars:
#		CYGPATH
#		EXEEXT
#	Defines only:
#		TEA_INITED
#		TEA_PLATFORM (windows or unix)
#
# "cygpath" is used on windows to generate native path names for include
# files. These variables should only be used with the compiler and linker
# since they generate native path names.
#
# EXEEXT
#	Select the executable extension based on the host type.  This
#	is a lightweight replacement for AC_EXEEXT that doesn't require
#	a compiler.
#------------------------------------------------------------------------

AC_DEFUN(TEA_INIT, [
    # TEA extensions pass this us the version of TEA they think they
    # are compatible with.
    TEA_VERSION="3.2"

    AC_MSG_CHECKING([for correct TEA configuration])
    if test x"${PACKAGE_NAME}" = x ; then
	AC_MSG_ERROR([
The PACKAGE_NAME variable must be defined by your TEA configure.in])
    fi
    if test x"$1" = x ; then
	AC_MSG_ERROR([
TEA version not specified.])
    elif test "$1" != "${TEA_VERSION}" ; then
	AC_MSG_RESULT([warning: requested TEA version "$1", have "${TEA_VERSION}"])
    else
	AC_MSG_RESULT([ok (TEA ${TEA_VERSION})])
    fi
    case "`uname -s`" in
	*win32*|*WIN32*|*CYGWIN_NT*|*CYGWIN_9*|*CYGWIN_ME*|*MINGW32_*)
	    AC_CHECK_PROG(CYGPATH, cygpath, cygpath -w, echo)
	    EXEEXT=".exe"
	    TEA_PLATFORM="windows"
	    ;;
	*)
	    CYGPATH=echo
	    EXEEXT=""
	    TEA_PLATFORM="unix"
	    ;;
    esac

    # Check if exec_prefix is set. If not use fall back to prefix.
    # Note when adjusted, so that TEA_PREFIX can correct for this.
    # This is needed for recursive configures, since autoconf propagates
    # $prefix, but not $exec_prefix (doh!).
    if test x$exec_prefix = xNONE ; then
	exec_prefix_default=yes
	exec_prefix=$prefix
    fi

    AC_SUBST(EXEEXT)
    AC_SUBST(CYGPATH)

    # This package name must be replaced statically for AC_SUBST to work
    AC_SUBST(PKG_LIB_FILE)
    # Substitute STUB_LIB_FILE in case package creates a stub library too.
    AC_SUBST(PKG_STUB_LIB_FILE)

    # We AC_SUBST these here to ensure they are subst'ed,
    # in case the user doesn't call TEA_ADD_...
    AC_SUBST(PKG_STUB_SOURCES)
    AC_SUBST(PKG_STUB_OBJECTS)
    AC_SUBST(PKG_TCL_SOURCES)
    AC_SUBST(PKG_HEADERS)
    AC_SUBST(PKG_INCLUDES)
    AC_SUBST(PKG_LIBS)
    AC_SUBST(PKG_CFLAGS)
])

#------------------------------------------------------------------------
# TEA_ADD_SOURCES --
#
#	Specify one or more source files.  Users should check for
#	the right platform before adding to their list.
#	It is not important to specify the directory, as long as it is
#	in the generic, win or unix subdirectory of $(srcdir).
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_SOURCES
#		PKG_OBJECTS
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_SOURCES, [
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
		if test x"${OBJEXT}" != x ; then
		    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.${OBJEXT}"
		else
		    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.\${OBJEXT}"
		fi
		PKG_OBJECTS="$PKG_OBJECTS $j"
		;;
	esac
    done
    AC_SUBST(PKG_SOURCES)
    AC_SUBST(PKG_OBJECTS)
])

#------------------------------------------------------------------------
# TEA_ADD_STUB_SOURCES --
#
#	Specify one or more source files.  Users should check for
#	the right platform before adding to their list.
#	It is not important to specify the directory, as long as it is
#	in the generic, win or unix subdirectory of $(srcdir).
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_STUB_SOURCES
#		PKG_STUB_OBJECTS
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_STUB_SOURCES, [
    vars="$@"
    for i in $vars; do
	# check for existence - allows for generic/win/unix VPATH
	if test ! -f "${srcdir}/$i" -a ! -f "${srcdir}/generic/$i" \
	    -a ! -f "${srcdir}/win/$i" -a ! -f "${srcdir}/unix/$i" \
	    ; then
	    AC_MSG_ERROR([could not find stub source file '$i'])
	fi
	PKG_STUB_SOURCES="$PKG_STUB_SOURCES $i"
	# this assumes it is in a VPATH dir
	i=`basename $i`
	# handle user calling this before or after TEA_SETUP_COMPILER
	if test x"${OBJEXT}" != x ; then
	    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.${OBJEXT}"
	else
	    j="`echo $i | sed -e 's/\.[[^.]]*$//'`.\${OBJEXT}"
	fi
	PKG_STUB_OBJECTS="$PKG_STUB_OBJECTS $j"
    done
    AC_SUBST(PKG_STUB_SOURCES)
    AC_SUBST(PKG_STUB_OBJECTS)
])

#------------------------------------------------------------------------
# TEA_ADD_TCL_SOURCES --
#
#	Specify one or more Tcl source files.  These should be platform
#	independent runtime files.
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_TCL_SOURCES
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_TCL_SOURCES, [
    vars="$@"
    for i in $vars; do
	# check for existence, be strict because it is installed
	if test ! -f "${srcdir}/$i" ; then
	    AC_MSG_ERROR([could not find tcl source file '${srcdir}/$i'])
	fi
	PKG_TCL_SOURCES="$PKG_TCL_SOURCES $i"
    done
    AC_SUBST(PKG_TCL_SOURCES)
])

#------------------------------------------------------------------------
# TEA_ADD_HEADERS --
#
#	Specify one or more source headers.  Users should check for
#	the right platform before adding to their list.
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_HEADERS
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_HEADERS, [
    vars="$@"
    for i in $vars; do
	# check for existence, be strict because it is installed
	if test ! -f "${srcdir}/$i" ; then
	    AC_MSG_ERROR([could not find header file '${srcdir}/$i'])
	fi
	PKG_HEADERS="$PKG_HEADERS $i"
    done
    AC_SUBST(PKG_HEADERS)
])

#------------------------------------------------------------------------
# TEA_ADD_INCLUDES --
#
#	Specify one or more include dirs.  Users should check for
#	the right platform before adding to their list.
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_INCLUDES
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_INCLUDES, [
    vars="$@"
    for i in $vars; do
	PKG_INCLUDES="$PKG_INCLUDES $i"
    done
    AC_SUBST(PKG_INCLUDES)
])

#------------------------------------------------------------------------
# TEA_ADD_LIBS --
#
#	Specify one or more libraries.  Users should check for
#	the right platform before adding to their list.  For Windows,
#	libraries provided in "foo.lib" format will be converted to
#	"-lfoo" when using GCC (mingw).
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_LIBS
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_LIBS, [
    vars="$@"
    for i in $vars; do
	if test "${TEA_PLATFORM}" = "windows" -a "$GCC" = "yes" ; then
	    # Convert foo.lib to -lfoo for GCC.  No-op if not *.lib
	    i=`echo "$i" | sed -e 's/^\([[^-]].*\)\.lib[$]/-l\1/i'`
	fi
	PKG_LIBS="$PKG_LIBS $i"
    done
    AC_SUBST(PKG_LIBS)
])

#------------------------------------------------------------------------
# TEA_ADD_CFLAGS --
#
#	Specify one or more CFLAGS.  Users should check for
#	the right platform before adding to their list.
#
# Arguments:
#	one or more file names
#
# Results:
#
#	Defines and substs the following vars:
#		PKG_CFLAGS
#------------------------------------------------------------------------
AC_DEFUN(TEA_ADD_CFLAGS, [
    PKG_CFLAGS="$PKG_CFLAGS $@"
    AC_SUBST(PKG_CFLAGS)
])

#------------------------------------------------------------------------
# TEA_PREFIX --
#
#	Handle the --prefix=... option by defaulting to what Tcl gave
#
# Arguments:
#	none
#
# Results:
#
#	If --prefix or --exec-prefix was not specified, $prefix and
#	$exec_prefix will be set to the values given to Tcl when it was
#	configured.
#------------------------------------------------------------------------
AC_DEFUN(TEA_PREFIX, [
    if test "${prefix}" = "NONE"; then
	prefix_default=yes
	if test x"${TCL_PREFIX}" != x; then
	    AC_MSG_NOTICE([--prefix defaulting to TCL_PREFIX ${TCL_PREFIX}])
	    prefix=${TCL_PREFIX}
	else
	    AC_MSG_NOTICE([--prefix defaulting to /usr/local])
	    prefix=/usr/local
	fi
    fi
    if test "${exec_prefix}" = "NONE" -a x"${prefix_default}" = x"yes" \
	-o x"${exec_prefix_default}" = x"yes" ; then
	if test x"${TCL_EXEC_PREFIX}" != x; then
	    AC_MSG_NOTICE([--exec-prefix defaulting to TCL_EXEC_PREFIX ${TCL_EXEC_PREFIX}])
	    exec_prefix=${TCL_EXEC_PREFIX}
	else
	    AC_MSG_NOTICE([--exec-prefix defaulting to ${prefix}])
	    exec_prefix=$prefix
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_SETUP_COMPILER_CC --
#
#	Do compiler checks the way we want.  This is just a replacement
#	for AC_PROG_CC in TEA configure.in files to make them cleaner.
#
# Arguments:
#	none
#
# Results:
#
#	Sets up CC var and other standard bits we need to make executables.
#------------------------------------------------------------------------
AC_DEFUN(TEA_SETUP_COMPILER_CC, [
    # Don't put any macros that use the compiler (e.g. AC_TRY_COMPILE)
    # in this macro, they need to go into TEA_SETUP_COMPILER instead.

    # If the user did not set CFLAGS, set it now to keep
    # the AC_PROG_CC macro from adding "-g -O2".
    if test "${CFLAGS+set}" != "set" ; then
	CFLAGS=""
    fi

    AC_PROG_CC
    AC_PROG_CPP

    AC_PROG_INSTALL

    #--------------------------------------------------------------------
    # Checks to see if the make program sets the $MAKE variable.
    #--------------------------------------------------------------------

    AC_PROG_MAKE_SET

    #--------------------------------------------------------------------
    # Find ranlib
    #--------------------------------------------------------------------

    AC_PROG_RANLIB

    #--------------------------------------------------------------------
    # Determines the correct binary file extension (.o, .obj, .exe etc.)
    #--------------------------------------------------------------------

    AC_OBJEXT
    AC_EXEEXT
])

#------------------------------------------------------------------------
# TEA_SETUP_COMPILER --
#
#	Do compiler checks that use the compiler.  This must go after
#	TEA_SETUP_COMPILER_CC, which does the actual compiler check.
#
# Arguments:
#	none
#
# Results:
#
#	Sets up CC var and other standard bits we need to make executables.
#------------------------------------------------------------------------
AC_DEFUN(TEA_SETUP_COMPILER, [
    # Any macros that use the compiler (e.g. AC_TRY_COMPILE) have to go here.
    AC_REQUIRE([TEA_SETUP_COMPILER_CC])

    #------------------------------------------------------------------------
    # If we're using GCC, see if the compiler understands -pipe. If so, use it.
    # It makes compiling go faster.  (This is only a performance feature.)
    #------------------------------------------------------------------------

    if test -z "$no_pipe" -a -n "$GCC"; then
	AC_MSG_CHECKING([if the compiler understands -pipe])
	OLDCC="$CC"
	CC="$CC -pipe"
	AC_TRY_COMPILE(,, AC_MSG_RESULT([yes]), CC="$OLDCC"
	    AC_MSG_RESULT([no]))
    fi

    #--------------------------------------------------------------------
    # Common compiler flag setup
    #--------------------------------------------------------------------

    AC_C_BIGENDIAN
    if test "${TEA_PLATFORM}" = "unix" ; then
	TEA_TCL_LINK_LIBS
	TEA_MISSING_POSIX_HEADERS
	# Let the user call this, because if it triggers, they will
	# need a compat/strtod.c that is correct.  Users can also
	# use Tcl_GetDouble(FromObj) instead.
	#TEA_BUGGY_STRTOD
    fi
])

#------------------------------------------------------------------------
# TEA_MAKE_LIB --
#
#	Generate a line that can be used to build a shared/unshared library
#	in a platform independent manner.
#
# Arguments:
#	none
#
#	Requires:
#
# Results:
#
#	Defines the following vars:
#	CFLAGS -	Done late here to note disturb other AC macros
#       MAKE_LIB -      Command to execute to build the Tcl library;
#                       differs depending on whether or not Tcl is being
#                       compiled as a shared library.
#	MAKE_SHARED_LIB	Makefile rule for building a shared library
#	MAKE_STATIC_LIB	Makefile rule for building a static library
#	MAKE_STUB_LIB	Makefile rule for building a stub library
#------------------------------------------------------------------------

AC_DEFUN(TEA_MAKE_LIB, [
    if test "${TEA_PLATFORM}" = "windows" -a "$GCC" != "yes"; then
	MAKE_STATIC_LIB="\${STLIB_LD} -out:\[$]@ \$(PKG_OBJECTS)"
	MAKE_SHARED_LIB="\${SHLIB_LD} \${SHLIB_LD_LIBS} \${LDFLAGS_DEFAULT} -out:\[$]@ \$(PKG_OBJECTS)"
	MAKE_STUB_LIB="\${STLIB_LD} -out:\[$]@ \$(PKG_STUB_OBJECTS)"
    else
	MAKE_STATIC_LIB="\${STLIB_LD} \[$]@ \$(PKG_OBJECTS)"
	MAKE_SHARED_LIB="\${SHLIB_LD} -o \[$]@ \$(PKG_OBJECTS) \${SHLIB_LD_LIBS}"
	MAKE_STUB_LIB="\${STLIB_LD} \[$]@ \$(PKG_STUB_OBJECTS)"
    fi

    if test "${SHARED_BUILD}" = "1" ; then
	MAKE_LIB="${MAKE_SHARED_LIB} "
    else
	MAKE_LIB="${MAKE_STATIC_LIB} "
    fi

    #--------------------------------------------------------------------
    # Shared libraries and static libraries have different names.
    # Use the double eval to make sure any variables in the suffix is
    # substituted. (@@@ Might not be necessary anymore)
    #--------------------------------------------------------------------

    if test "${TEA_PLATFORM}" = "windows" ; then
	if test "${SHARED_BUILD}" = "1" ; then
	    # We force the unresolved linking of symbols that are really in
	    # the private libraries of Tcl and Tk.
	    SHLIB_LD_LIBS="${SHLIB_LD_LIBS} \"`${CYGPATH} ${TCL_BIN_DIR}/${TCL_STUB_LIB_FILE}`\""
	    if test x"${TK_BIN_DIR}" != x ; then
		SHLIB_LD_LIBS="${SHLIB_LD_LIBS} \"`${CYGPATH} ${TK_BIN_DIR}/${TK_STUB_LIB_FILE}`\""
	    fi
	    eval eval "PKG_LIB_FILE=${PACKAGE_NAME}${SHARED_LIB_SUFFIX}"
	else
	    eval eval "PKG_LIB_FILE=${PACKAGE_NAME}${UNSHARED_LIB_SUFFIX}"
	fi
	# Some packages build there own stubs libraries
	eval eval "PKG_STUB_LIB_FILE=${PACKAGE_NAME}stub${UNSHARED_LIB_SUFFIX}"
	# These aren't needed on Windows (either MSVC or gcc)
	RANLIB=:
	RANLIB_STUB=:
    else
	RANLIB_STUB="${RANLIB}"
	if test "${SHARED_BUILD}" = "1" ; then
	    SHLIB_LD_LIBS="${SHLIB_LD_LIBS} ${TCL_STUB_LIB_SPEC}"
	    if test x"${TK_BIN_DIR}" != x ; then
		SHLIB_LD_LIBS="${SHLIB_LD_LIBS} ${TK_STUB_LIB_SPEC}"
	    fi
	    eval eval "PKG_LIB_FILE=lib${PACKAGE_NAME}${SHARED_LIB_SUFFIX}"
	    RANLIB=:
	else
	    eval eval "PKG_LIB_FILE=lib${PACKAGE_NAME}${UNSHARED_LIB_SUFFIX}"
	fi
	# Some packages build there own stubs libraries
	eval eval "PKG_STUB_LIB_FILE=lib${PACKAGE_NAME}stub${UNSHARED_LIB_SUFFIX}"
    fi

    # These are escaped so that only CFLAGS is picked up at configure time.
    # The other values will be substituted at make time.
    CFLAGS="${CFLAGS} \${CFLAGS_DEFAULT} \${CFLAGS_WARNING}"
    if test "${SHARED_BUILD}" = "1" ; then
	CFLAGS="${CFLAGS} \${SHLIB_CFLAGS}"
    fi

    AC_SUBST(MAKE_LIB)
    AC_SUBST(MAKE_SHARED_LIB)
    AC_SUBST(MAKE_STATIC_LIB)
    AC_SUBST(MAKE_STUB_LIB)
    AC_SUBST(RANLIB_STUB)
])

#------------------------------------------------------------------------
# TEA_LIB_SPEC --
#
#	Compute the name of an existing object library located in libdir
#	from the given base name and produce the appropriate linker flags.
#
# Arguments:
#	basename	The base name of the library without version
#			numbers, extensions, or "lib" prefixes.
#	extra_dir	Extra directory in which to search for the
#			library.  This location is used first, then
#			$prefix/$exec-prefix, then some defaults.
#
# Requires:
#	TEA_INIT and TEA_PREFIX must be called first.
#
# Results:
#
#	Defines the following vars:
#		${basename}_LIB_NAME	The computed library name.
#		${basename}_LIB_SPEC	The computed linker flags.
#------------------------------------------------------------------------

AC_DEFUN(TEA_LIB_SPEC, [
    AC_MSG_CHECKING([for $1 library])

    # Look in exec-prefix for the library (defined by TEA_PREFIX).

    tea_lib_name_dir="${exec_prefix}/lib"

    # Or in a user-specified location.

    if test x"$2" != x ; then
	tea_extra_lib_dir=$2
    else
	tea_extra_lib_dir=NONE
    fi

    for i in \
	    `ls -dr ${tea_extra_lib_dir}/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr ${tea_extra_lib_dir}/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr ${tea_lib_name_dir}/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr ${tea_lib_name_dir}/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr /usr/lib/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr /usr/lib/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr /usr/local/lib/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr /usr/local/lib/lib$1[[0-9]]* 2>/dev/null ` ; do
	if test -f "$i" ; then
	    tea_lib_name_dir=`dirname $i`
	    $1_LIB_NAME=`basename $i`
	    $1_LIB_PATH_NAME=$i
	    break
	fi
    done

    if test "${TEA_PLATFORM}" = "windows"; then
	$1_LIB_SPEC=\"`${CYGPATH} ${$1_LIB_PATH_NAME} 2>/dev/null`\"
    else
	# Strip off the leading "lib" and trailing ".a" or ".so"

	tea_lib_name_lib=`echo ${$1_LIB_NAME}|sed -e 's/^lib//' -e 's/\.[[^.]]*$//' -e 's/\.so.*//'`
	$1_LIB_SPEC="-L${tea_lib_name_dir} -l${tea_lib_name_lib}"
    fi

    if test "x${$1_LIB_NAME}" = x ; then
	AC_MSG_ERROR([not found])
    else
	AC_MSG_RESULT([${$1_LIB_SPEC}])
    fi
])

#------------------------------------------------------------------------
# TEA_PRIVATE_TCL_HEADERS --
#
#	Locate the private Tcl include files
#
# Arguments:
#
#	Requires:
#		TCL_SRC_DIR	Assumes that TEA_LOAD_TCLCONFIG has
#				 already been called.
#
# Results:
#
#	Substs the following vars:
#		TCL_TOP_DIR_NATIVE
#		TCL_GENERIC_DIR_NATIVE
#		TCL_UNIX_DIR_NATIVE
#		TCL_WIN_DIR_NATIVE
#		TCL_BMAP_DIR_NATIVE
#		TCL_TOOL_DIR_NATIVE
#		TCL_PLATFORM_DIR_NATIVE
#		TCL_BIN_DIR_NATIVE
#		TCL_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PRIVATE_TCL_HEADERS, [
    AC_MSG_CHECKING([for Tcl private include files])

    if test "${TEA_PLATFORM}" = "windows"; then
	TCL_TOP_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}`\"
	TCL_GENERIC_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/generic`\"
	TCL_UNIX_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/unix`\"
	TCL_WIN_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/win`\"
	TCL_BMAP_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/bitmaps`\"
	TCL_TOOL_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/tools`\"
	TCL_COMPAT_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/compat`\"
	TCL_PLATFORM_DIR_NATIVE=${TCL_WIN_DIR_NATIVE}

	TCL_INCLUDES="-I${TCL_GENERIC_DIR_NATIVE} -I${TCL_PLATFORM_DIR_NATIVE}"
    else
	TCL_TOP_DIR_NATIVE='$(TCL_SRC_DIR)'
	TCL_GENERIC_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/generic'
	TCL_UNIX_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/unix'
	TCL_WIN_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/win'
	TCL_BMAP_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/bitmaps'
	TCL_TOOL_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/tools'
	TCL_COMPAT_DIR_NATIVE='${TCL_TOP_DIR_NATIVE}/compat'
	TCL_PLATFORM_DIR_NATIVE=${TCL_UNIX_DIR_NATIVE}

	# substitute these in "relaxed" so that TCL_INCLUDES still works
	# without requiring the other vars to be defined in the Makefile
	eval "TCL_INCLUDES=\"-I${TCL_GENERIC_DIR_NATIVE} -I${TCL_PLATFORM_DIR_NATIVE}\""
    fi

    AC_SUBST(TCL_TOP_DIR_NATIVE)
    AC_SUBST(TCL_GENERIC_DIR_NATIVE)
    AC_SUBST(TCL_UNIX_DIR_NATIVE)
    AC_SUBST(TCL_WIN_DIR_NATIVE)
    AC_SUBST(TCL_BMAP_DIR_NATIVE)
    AC_SUBST(TCL_TOOL_DIR_NATIVE)
    AC_SUBST(TCL_PLATFORM_DIR_NATIVE)

    AC_SUBST(TCL_INCLUDES)
    AC_MSG_RESULT([Using srcdir found in tclConfig.sh: ${TCL_SRC_DIR}])
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TCL_HEADERS --
#
#	Locate the installed public Tcl header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tclinclude switch to configure.
#	Result is cached.
#
#	Substs the following vars:
#		TCL_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PUBLIC_TCL_HEADERS, [
    AC_MSG_CHECKING([for Tcl public headers])

    AC_ARG_WITH(tclinclude, [  --with-tclinclude       directory containing the public Tcl header files], with_tclinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tclh, [
	# Use the value from --with-tclinclude, if it was given

	if test x"${with_tclinclude}" != x ; then
	    if test -f "${with_tclinclude}/tcl.h" ; then
		ac_cv_c_tclh=${with_tclinclude}
	    else
		AC_MSG_ERROR([${with_tclinclude} directory does not contain tcl.h])
	    fi
	else
	    # Check order: pkg --prefix location, Tcl's --prefix location,
	    # directory of tclConfig.sh, and Tcl source directory.
	    # Looking in the source dir is not ideal, but OK.

	    eval "temp_includedir=${includedir}"
	    list="`ls -d ${temp_includedir}      2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null` \
		`ls -d ${TCL_SRC_DIR}/generic    2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
	    fi
	    for i in $list ; do
		if test -f "$i/tcl.h" ; then
		    ac_cv_c_tclh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tclh}" = x ; then
	AC_MSG_ERROR([tcl.h not found.  Please specify its location with --with-tclinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tclh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tclh}`

    TCL_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TCL_INCLUDES)
])

#------------------------------------------------------------------------
# TEA_PRIVATE_TK_HEADERS --
#
#	Locate the private Tk include files
#
# Arguments:
#
#	Requires:
#		TK_SRC_DIR	Assumes that TEA_LOAD_TKCONFIG has
#				 already been called.
#
# Results:
#
#	Substs the following vars:
#		TK_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PRIVATE_TK_HEADERS, [
    AC_MSG_CHECKING([for Tk private include files])

    if test "${TEA_PLATFORM}" = "windows"; then
	TK_TOP_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}`\"
	TK_UNIX_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/unix`\"
	TK_WIN_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/win`\"
	TK_GENERIC_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/generic`\"
	TK_XLIB_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/xlib`\"
	TK_PLATFORM_DIR_NATIVE=${TK_WIN_DIR_NATIVE}

	TK_INCLUDES="-I${TK_GENERIC_DIR_NATIVE} -I${TK_PLATFORM_DIR_NATIVE} -I${TK_XLIB_DIR_NATIVE}"
    else
	TK_TOP_DIR_NATIVE='${TK_SRC_DIR}'
	TK_GENERIC_DIR_NATIVE='${TK_TOP_DIR_NATIVE}/generic'
	TK_UNIX_DIR_NATIVE='${TK_TOP_DIR_NATIVE}/unix'
	TK_WIN_DIR_NATIVE='${TK_TOP_DIR_NATIVE}/win'
	TK_PLATFORM_DIR_NATIVE=${TK_UNIX_DIR_NATIVE}

	# substitute these in "relaxed" so that TK_INCLUDES still works
	# without requiring the other vars to be defined in the Makefile
	eval "TK_INCLUDES=\"-I${TK_GENERIC_DIR_NATIVE} -I${TK_PLATFORM_DIR_NATIVE}\""
    fi

    AC_SUBST(TK_TOP_DIR_NATIVE)
    AC_SUBST(TK_UNIX_DIR_NATIVE)
    AC_SUBST(TK_WIN_DIR_NATIVE)
    AC_SUBST(TK_GENERIC_DIR_NATIVE)
    AC_SUBST(TK_XLIB_DIR_NATIVE)
    AC_SUBST(TK_PLATFORM_DIR_NATIVE)

    AC_SUBST(TK_INCLUDES)
    AC_MSG_RESULT([Using srcdir found in tkConfig.sh: ${TK_SRC_DIR}])
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TK_HEADERS --
#
#	Locate the installed public Tk header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tkinclude switch to configure.
#	Result is cached.
#
#	Substs the following vars:
#		TK_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PUBLIC_TK_HEADERS, [
    AC_MSG_CHECKING([for Tk public headers])

    AC_ARG_WITH(tkinclude, [  --with-tkinclude      directory containing the public Tk header files.], with_tkinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tkh, [
	# Use the value from --with-tkinclude, if it was given

	if test x"${with_tkinclude}" != x ; then
	    if test -f "${with_tkinclude}/tk.h" ; then
		ac_cv_c_tkh=${with_tkinclude}
	    else
		AC_MSG_ERROR([${with_tkinclude} directory does not contain tk.h])
	    fi
	else
	    # Check order: pkg --prefix location, Tcl's --prefix location,
	    # directory of tclConfig.sh, and Tcl source directory.
	    # Looking in the source dir is not ideal, but OK.

	    eval "temp_includedir=${includedir}"
	    list="`ls -d ${temp_includedir}      2>/dev/null` \
		`ls -d ${TK_PREFIX}/include      2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null` \
		`ls -d ${TK_SRC_DIR}/generic     2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
	    fi
	    for i in $list ; do
		if test -f "$i/tk.h" ; then
		    ac_cv_c_tkh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tkh}" = x ; then
	AC_MSG_ERROR([tk.h not found.  Please specify its location with --with-tkinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tkh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tkh}`

    TK_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TK_INCLUDES)

    if test "${TEA_PLATFORM}" = "windows" ; then
	# On Windows, we need the X compat headers
	AC_MSG_CHECKING([for X11 header files])
	if test ! -r "${INCLUDE_DIR_NATIVE}/X11/Xlib.h"; then
	    INCLUDE_DIR_NATIVE="`${CYGPATH} ${TK_SRC_DIR}/xlib`"
	    TK_XINCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"
	    AC_SUBST(TK_XINCLUDES)
	fi
	AC_MSG_RESULT([${INCLUDE_DIR_NATIVE}])
    fi
])

#------------------------------------------------------------------------
# TEA_PROG_TCLSH
#	Locate a tclsh shell in the following directories:
#		${TCL_BIN_DIR}		${TCL_BIN_DIR}/../bin
#		${exec_prefix}/bin	${prefix}/bin
#		${PATH}
#
# Arguments
#	none
#
# Results
#	Subst's the following values:
#		TCLSH_PROG
#------------------------------------------------------------------------

AC_DEFUN(TEA_PROG_TCLSH, [
    # Allow the user to provide this setting in the env
    if test "x${TCLSH_PROG}" = "x" ; then
	AC_MSG_CHECKING([for tclsh])

	AC_CACHE_VAL(ac_cv_path_tclsh, [
	search_path=`echo ${PATH} | sed -e 's/:/ /g'`
	if test "${TEA_PLATFORM}" != "windows" -o \
		\( "$do64bit_ok" = "no" -a "$doWince" = "no" \) ; then
	    # Do not allow target tclsh in known cross-compile builds,
	    # as we need one we can run on this system
	    search_path="${TCL_BIN_DIR} ${TCL_BIN_DIR}/../bin ${exec_prefix}/bin ${prefix}/bin ${search_path}"
	fi
	for dir in $search_path ; do
	    for j in `ls -r $dir/tclsh[[8-9]]*${EXEEXT} 2> /dev/null` \
		    `ls -r $dir/tclsh*${EXEEXT} 2> /dev/null` ; do
		if test x"$ac_cv_path_tclsh" = x ; then
		    if test -f "$j" ; then
			ac_cv_path_tclsh=$j
			break
		    fi
		fi
	    done
	done
	])

	if test -f "$ac_cv_path_tclsh" ; then
	    TCLSH_PROG=$ac_cv_path_tclsh
	    AC_MSG_RESULT([$TCLSH_PROG])
	else
	    AC_MSG_ERROR([No tclsh found in PATH: $search_path])
	fi
    fi
    AC_SUBST(TCLSH_PROG)
])

#------------------------------------------------------------------------
# TEA_PROG_WISH
#	Locate a wish shell in the following directories:
#		${TK_BIN_DIR}		${TK_BIN_DIR}/../bin
#		${TCL_BIN_DIR}		${TCL_BIN_DIR}/../bin
#		${exec_prefix}/bin	${prefix}/bin
#		${PATH}
#
# Arguments
#	none
#
# Results
#	Subst's the following values:
#		WISH_PROG
#------------------------------------------------------------------------

AC_DEFUN(TEA_PROG_WISH, [
    # Allow the user to provide this setting in the env
    if test "x${WISH_PROG}" = "x" ; then
	AC_MSG_CHECKING([for wish])

	AC_CACHE_VAL(ac_cv_path_wish, [
	search_path=`echo ${PATH} | sed -e 's/:/ /g'`
	if test "${TEA_PLATFORM}" != "windows" -o \
		\( "$do64bit_ok" = "no" -a "$doWince" = "no" \) ; then
	    # Do not allow target wish in known cross-compile builds,
	    # as we need one we can run on this system
	    search_path="${TK_BIN_DIR} ${TK_BIN_DIR}/../bin ${TCL_BIN_DIR} ${TCL_BIN_DIR}/../bin ${exec_prefix}/bin ${prefix}/bin ${search_path}"
	fi
	for dir in $search_path ; do
	    for j in `ls -r $dir/wish[[8-9]]*${EXEEXT} 2> /dev/null` \
		    `ls -r $dir/wish*${EXEEXT} 2> /dev/null` ; do
		if test x"$ac_cv_path_wish" = x ; then
		    if test -f "$j" ; then
			ac_cv_path_wish=$j
			break
		    fi
		fi
	    done
	done
	])

	if test -f "$ac_cv_path_wish" ; then
	WISH_PROG=$ac_cv_path_wish
	    AC_MSG_RESULT([$WISH_PROG])
	else
	    AC_MSG_ERROR([No wish found in PATH: $search_path])
	fi
    fi
    AC_SUBST(WISH_PROG)
])

#------------------------------------------------------------------------
# TEA_PATH_CONFIG --
#
#	Locate the ${1}Config.sh file and perform a sanity check on
#	the ${1} compile flags.  These are used by packages like
#	[incr Tk] that load *Config.sh files from more than Tcl and Tk.
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-$1=...
#
#	Defines the following vars:
#		$1_BIN_DIR	Full path to the directory containing
#				the $1Config.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_CONFIG, [
    #
    # Ok, lets find the $1 configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-$1
    #

    if test x"${no_$1}" = x ; then
	# we reset no_$1 in case something fails here
	no_$1=true
	AC_ARG_WITH($1, [  --with-$1              directory containing $1 configuration ($1Config.sh)], with_$1config=${withval})
	AC_MSG_CHECKING([for $1 configuration])
	AC_CACHE_VAL(ac_cv_c_$1config,[

	    # First check to see if --with-$1 was specified.
	    if test x"${with_$1config}" != x ; then
		case ${with_$1config} in
		    */$1Config.sh )
			if test -f ${with_$1config}; then
			    AC_MSG_WARN([--with-$1 argument should refer to directory containing $1Config.sh, not to $1Config.sh itself])
			    with_$1config=`echo ${with_$1config} | sed 's!/$1Config\.sh$!!'`
			fi;;
		esac
		if test -f "${with_$1config}/$1Config.sh" ; then
		    ac_cv_c_$1config=`(cd ${with_$1config}; pwd)`
		else
		    AC_MSG_ERROR([${with_$1config} directory doesn't contain $1Config.sh])
		fi
	    fi

	    # then check for a private $1 installation
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in \
			../$1 \
			`ls -dr ../$1*[[0-9]].[[0-9]]*.[[0-9]]* 2>/dev/null` \
			`ls -dr ../$1*[[0-9]].[[0-9]][[0-9]] 2>/dev/null` \
			`ls -dr ../$1*[[0-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../$1*[[0-9]].[[0-9]]* 2>/dev/null` \
			../../$1 \
			`ls -dr ../../$1*[[0-9]].[[0-9]]*.[[0-9]]* 2>/dev/null` \
			`ls -dr ../../$1*[[0-9]].[[0-9]][[0-9]] 2>/dev/null` \
			`ls -dr ../../$1*[[0-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../$1*[[0-9]].[[0-9]]* 2>/dev/null` \
			../../../$1 \
			`ls -dr ../../../$1*[[0-9]].[[0-9]]*.[[0-9]]* 2>/dev/null` \
			`ls -dr ../../../$1*[[0-9]].[[0-9]][[0-9]] 2>/dev/null` \
			`ls -dr ../../../$1*[[0-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../../$1*[[0-9]].[[0-9]]* 2>/dev/null` \
			${srcdir}/../$1 \
			`ls -dr ${srcdir}/../$1*[[0-9]].[[0-9]]*.[[0-9]]* 2>/dev/null` \
			`ls -dr ${srcdir}/../$1*[[0-9]].[[0-9]][[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../$1*[[0-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../$1*[[0-9]].[[0-9]]* 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		    if test -f "$i/unix/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	])

	if test x"${ac_cv_c_$1config}" = x ; then
	    $1_BIN_DIR="# no $1 configs found"
	    AC_MSG_WARN("Cannot find $1 configuration definitions")
	    exit 0
	else
	    no_$1=
	    $1_BIN_DIR=${ac_cv_c_$1config}
	    AC_MSG_RESULT([found $$1_BIN_DIR/$1Config.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_LOAD_CONFIG --
#
#	Load the $1Config.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		$1_BIN_DIR
#
# Results:
#
#	Subst the following vars:
#		$1_SRC_DIR
#		$1_LIB_FILE
#		$1_LIB_SPEC
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_CONFIG, [
    AC_MSG_CHECKING([for existence of ${$1_BIN_DIR}/$1Config.sh])

    if test -f "${$1_BIN_DIR}/$1Config.sh" ; then
        AC_MSG_RESULT([loading])
	. ${$1_BIN_DIR}/$1Config.sh
    else
        AC_MSG_RESULT([file not found])
    fi

    #
    # If the $1_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable $1_LIB_SPEC will be set to the value
    # of $1_BUILD_LIB_SPEC. An extension should make use of $1_LIB_SPEC
    # instead of $1_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f ${$1_BIN_DIR}/Makefile ; then
	AC_MSG_WARN([Found Makefile - using build library specs for $1])
        $1_LIB_SPEC=${$1_BUILD_LIB_SPEC}
        $1_STUB_LIB_SPEC=${$1_BUILD_STUB_LIB_SPEC}
        $1_STUB_LIB_PATH=${$1_BUILD_STUB_LIB_PATH}
    fi

    AC_SUBST($1_VERSION)
    AC_SUBST($1_BIN_DIR)
    AC_SUBST($1_SRC_DIR)

    AC_SUBST($1_LIB_FILE)
    AC_SUBST($1_LIB_SPEC)

    AC_SUBST($1_STUB_LIB_FILE)
    AC_SUBST($1_STUB_LIB_SPEC)
    AC_SUBST($1_STUB_LIB_PATH)
])

#------------------------------------------------------------------------
# TEA_PATH_CELIB --
#
#	Locate Keuchel's celib emulation layer for targeting Win/CE
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-celib=...
#
#	Defines the following vars:
#		CELIB_DIR	Full path to the directory containing
#				the include and platform lib files
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_CELIB, [
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-celib

    if test x"${no_celib}" = x ; then
	# we reset no_celib in case something fails here
	no_celib=true
	AC_ARG_WITH(celib,[  --with-celib=DIR        use Windows/CE support library from DIR], with_celibconfig=${withval})
	AC_MSG_CHECKING([for Windows/CE celib directory])
	AC_CACHE_VAL(ac_cv_c_celibconfig,[
	    # First check to see if --with-celibconfig was specified.
	    if test x"${with_celibconfig}" != x ; then
		if test -d "${with_celibconfig}/inc" ; then
		    ac_cv_c_celibconfig=`(cd ${with_celibconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_celibconfig} directory doesn't contain inc directory])
		fi
	    fi

	    # then check for a celib library
	    if test x"${ac_cv_c_celibconfig}" = x ; then
		for i in \
			../celib-palm-3.0 \
			../celib \
			../../celib-palm-3.0 \
			../../celib \
			`ls -dr ../celib-*3.[[0-9]]* 2>/dev/null` \
			${srcdir}/../celib-palm-3.0 \
			${srcdir}/../celib \
			`ls -dr ${srcdir}/../celib-*3.[[0-9]]* 2>/dev/null` \
			; do
		    if test -d "$i/inc" ; then
			ac_cv_c_celibconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	])
	if test x"${ac_cv_c_celibconfig}" = x ; then
	    AC_MSG_ERROR([Cannot find celib support library directory])
	else
	    no_celib=
	    CELIB_DIR=${ac_cv_c_celibconfig}
	    AC_MSG_RESULT([found $CELIB_DIR])
	    TEA_PATH_NOSPACE(CELIB_DIR, ${ac_cv_c_celibconfig})
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_PATH_NOSPACE
#	Ensure that the given path has no spaces.  This is necessary for
#	CC (and consitutuent vars that build it up) to work in the
#	tortured autoconf environment.  Currently only for Windows use.
#
# Arguments
#	VAR  - name of the variable to set
#	PATH - path to ensure no spaces in
#
# Results
#	Sets $VAR to short path of $PATH if it can be found.
#------------------------------------------------------------------------

AC_DEFUN([TEA_PATH_NOSPACE], [
    if test "${TEA_PLATFORM}" = "windows" ; then
	# we need TCLSH_PROG defined to get Windows short pathnames
	AC_REQUIRE([TEA_PROG_TCLSH])

	AC_MSG_CHECKING([short pathname for $1 ($2)])

	shortpath=
	case "$2" in
	    *\ *)
		# Only do this if we need to.
		shortpath=`echo "puts [[file attributes {$2} -shortname]] ; exit" | ${TCLSH_PROG} 2>/dev/null`
		;;
	esac
	if test "x${shortpath}" = "x" ; then
	    AC_MSG_RESULT([not changed])
	else
	    $1=$shortpath
	    AC_MSG_RESULT([${$1}])
	fi
    fi
])
