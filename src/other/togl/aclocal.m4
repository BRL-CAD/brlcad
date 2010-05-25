#
# Include the TEA standard macro set
#

builtin(include,tclconfig/tcl.m4)

#
# Add here whatever m4 macros you want to define for your package
#

#------------------------------------------------------------------------
# TOGL_ENABLE_STUBS --
#
#	Specifiy if stubs should be used.
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--enable-stubs
#
#------------------------------------------------------------------------

AC_DEFUN(TOGL_ENABLE_STUBS, [
    AC_MSG_CHECKING([whether to link with stubs library])
    AC_ARG_ENABLE(stubs,
	[  --enable-stubs          build and link with stub libraries (--enable-stubs)],
	[tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_stubs+set}" = set; then
	enableval="$enable_stubs"
	tcl_ok=$enableval
    else
	tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
	AC_MSG_RESULT([stubs])
	USE_STUBS=1
    else
	AC_MSG_RESULT([no stubs])
	USE_STUBS=0
    fi
])

#------------------------------------------------------------------------
# TOGL_UNDEF_GET_PROC_ADDRESS --
#
#	Does defining GLX_GLXEXT_LEGACY interfer with including GL/glxext.h?
#
# Arguments:
#	none
#
# Results:
#
#	defines TOGL_UNDEF_GET_PROC_ADDRESS
#
#------------------------------------------------------------------------
AC_DEFUN(TOGL_UNDEF_GET_PROC_ADDRESS, [
    AC_MSG_CHECKING([if GLX_GLXEXT_LEGACY interfers with including GL/glxext.h])
    AC_LANG_PUSH(C)
    ac_save_CFLAGS=$CFLAGS
    CFLAGS=$TK_XINCLUDES
    AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE([[
#define GLX_GLXEXT_LEGACY
#include <GL/glx.h>
#undef GLX_VERSION_1_3
#undef GLX_VERSION_1_4
#include <GL/glxext.h>
int main() { return 0; }
]])],
	[AC_MSG_RESULT([no])],
	[AC_MSG_RESULT([yes])
	 AC_DEFINE(UNDEF_GET_PROC_ADDRESS, 1)])
    CFLAGS=$ac_save_CFLAGS
    AC_LANG_POP()
])
