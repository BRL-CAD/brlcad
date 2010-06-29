# Game/graphics/multimedia oriented m4 package for autoconf/aclocal.
# 
# Copyright 2000-2007 Erik Greenwald <erik@smluc.org>
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# This product includes software developed by Erik Greenwald.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id$
#
# thumbs up to 'famp' at http://famp.sourceforge.net for providing an 
# example of how this is done.
#
# AC_SUBST's both GL_CFLAGS and GL_LIBS, and sets have_GL
#
# oct 26, 2000 
# * Changed CONFIGURE_OPENGL to AM_PATH_OPENGL for consistancy
# -Erik Greenwald <erik@smluc.org>

#
# AM_PATH_OPENGL([ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])
#

AC_DEFUN([AM_PATH_OPENGL],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_TARGET])

dnl I think this should be target for cross compilers, but krissa's busted
dnl install pukes on that. The things I do for pretty girls *sigh*
case $target in
	*-*-cygwin* | *-*-mingw*)
		;;
	*)
		# X and X-extra not needed for cyggy stuff
		AC_REQUIRE([AC_PATH_X])
		AC_REQUIRE([AC_PATH_XTRA])
		;;
esac

  GL_CFLAGS=""
  GL_LIBS=""

  GLXEXTRALIBSTUFF="-lX11 -lXext -lXt"

  AC_LANG_SAVE
  AC_LANG_C

  if ! test x"$no_x" = xyes; then
    GL_CFLAGS="$X_CFLAGS"
    GL_X_LIBS="$X_PRE_LIBS $X_LIBS $GLXEXTRALIBSTUFF $X_EXTRA_LIBS $LIBM"
  fi

  AC_ARG_WITH(gl-prefix,    
    [  --with-gl-prefix=PFX     Prefix where OpenGL is installed],
    [
      GL_CFLAGS="-I$withval/include"
      GL_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH(gl-includes,    
    [  --with-gl-includes=DIR   where the OpenGL includes are installed],
    [
      GL_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH(gl-libraries,    
    [  --with-gl-libraries=DIR  where the OpenGL libraries are installed],
    [
      GL_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  AC_LANG_SAVE
  AC_LANG_C
  have_GL=no

  exec 8>&AC_FD_MSG
dnl  exec AC_FD_MSG>/dev/null

dnl windows is fucking retarded. It can't cope with searchlib because of
dnl stupid WINGDAPI APIENTRY crap. :( So we punt.
dnl Also, Mrs Krissa had an issue with -I/usr/X11R6/include showing up on
dnl her compile line and pharking things up for link phase, so we clear that.
case "$target" in
	*-*-cygwin* | *-*-mingw32*)
		GL_CFLAGS=""
		GL_LIBS="-lopengl32"
		have_GL=yes
		;;
	*-apple-darwin*)
		GL_LIBS="-framework GLUT -framework OpenGL -framework Carbon -lobjc"
		have_GL=yes
		;;
	*)
		AC_SEARCH_LIBS(glFrustum,
			[GL MesaGL Mesa3d opengl opengl32],
			[
				have_GL=yes
				GL_LIBS="$GL_LIBS $LIBS"
			],
			AC_ERROR(Need OpenGL),
			$GL_LIBS $GL_X_LIBS)
		;;
esac

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  exec AC_FD_MSG>&8
  AC_LANG_RESTORE

  if test "$have_GL" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     GL_CFLAGS=""
     GL_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(GL_CFLAGS)
  AC_SUBST(GL_LIBS)
])

AC_DEFUN([AM_PATH_GLU],
[
  dnl *** bring in opengl as a dependancy ***
  AM_PATH_OPENGL

  GLU_CFLAGS=""
  GLU_LIBS=""

  AC_LANG_SAVE
  AC_LANG_C

  AC_ARG_WITH(glu-prefix,    
    [  --with-glu-prefix=PFX     Prefix where GLU is installed],
    [
      GLU_CFLAGS="-I$withval/include"
      GLU_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH(glu-includes,    
    [  --with-glu-includes=DIR   where the GLU includes are installed],
    [
      GLU_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH(glu-libraries,    
    [  --with-glu-libraries=DIR  where the GLU libraries are installed],
    [
      GLU_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  AC_LANG_SAVE
  AC_LANG_C
  have_GL=no

  exec 8>&AC_FD_MSG
dnl  exec AC_FD_MSG>/dev/null

case "$target" in
	*-*-cygwin* | *-*-mingw32*)
		GLU_CFLAGS=""
		GLU_LIBS="-lGLU32"
		have_GLU=yes
		;;
	*-apple-darwin*)
		dnl apple gets glut by default
		GLU_LIBS="$GL_LIBS"
		have_GLU=yes
		;;
	*)
		AC_SEARCH_LIBS(gluPerspective,
			[GLU MesaGLU glu glu32],
			[
				have_GLU=yes
				GLU_LIBS="$GLU_LIBS $LIBS"
			],
			AC_ERROR(Need GLU),
			$GLU_LIBS)
		;;
esac

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  exec AC_FD_MSG>&8
  AC_LANG_RESTORE

  if test "$have_GLU" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     GLU_CFLAGS=""
     GLU_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(GLU_CFLAGS)
  AC_SUBST(GLU_LIBS)
])

AC_DEFUN([AM_PATH_GLUT],
[
  dnl *** bring in glu as a dependancy ***
  AM_PATH_GLU

  GLUT_CFLAGS=""
  GLUT_LIBS=""

  AC_LANG_SAVE
  AC_LANG_C

  AC_ARG_WITH(glut-prefix,    
    [  --with-glut-prefix=PFX     Prefix where glut is installed],
    [
      GLUT_CFLAGS="-I$withval/include"
      GLUT_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH(glut-includes,    
    [  --with-glut-includes=DIR   where the glut includes are installed],
    [
      GLUT_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH(glut-libraries,    
    [  --with-glut-libraries=DIR  where the glut libraries are installed],
    [
      GLUT_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  AC_LANG_SAVE
  AC_LANG_C
  have_GL=no

  exec 8>&AC_FD_MSG
dnl  exec AC_FD_MSG>/dev/null

case "$target" in
	*-*-cygwin* | *-*-mingw32*)
		GLUT_CFLAGS=""
		GLUT_LIBS="-lGLUT32"
		have_GLUT=yes
		;;
	*-apple-darwin*)
		GLUT_LIBS="-framework GLUT $GL_LIBS"
		have_GLUT=yes
		;;
	*)
		AC_SEARCH_LIBS(glutInit,
			[glut],
			[
				have_GLUT=yes
				GLUT_LIBS="$GLUT_LIBS $LIBS"
			],
			AC_ERROR(Need GLUT),
			$GLUT_LIBS)
		;;
esac

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  exec AC_FD_MSG>&8
  AC_LANG_RESTORE

  if test "$have_GLUT" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     GLUT_CFLAGS=""
     GLUT_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(GLUT_CFLAGS)
  AC_SUBST(GLUT_LIBS)
])

