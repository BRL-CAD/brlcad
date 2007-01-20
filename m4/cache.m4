#                       C A C H E . M 4
# BRL-CAD
#
# Copyright (c) 2005-2007 United States Government as represented by
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
# BC_CONFIG_CACHE
#
# automatically enable and load the configure cache file if available
#
###

AC_DEFUN([BC_CONFIG_CACHE], [
AC_MSG_CHECKING([whether a configure cache exists])
if test "x$cache_file" = "x/dev/null" ; then
	configure_cache=ifelse([$1], , [config.cache.${host_os}], [$1])
	CONFIG_CACHE=""
	if test -f "$configure_cache"; then
		if test "x`cat $configure_cache | grep ac_cv_env_CC_value`" != "xac_cv_env_CC_value=$CC" ; then
			dnl if the compiler we're using now doesn't
			dnl match the compiler used in the previous
			dnl cached results, invalidate it.
			AC_MSG_RESULT([found but compiler differs])
			rm -f "$configure_cache"
		elif test "x`cat $configure_cache | grep ac_cv_env_CPPFLAGS_value`" != "xac_cv_env_CPPFLAGS_value=$CPPFLAGS" ; then
			dnl if the preprocessor flags we're using now
			dnl doesn't match the flags used in the
			dnl previous cached results, invalidate it.
			AC_MSG_RESULT([found but preprocessor flags differ])
			rm -f "$configure_cache"
		else
			dnl if the configure script has been modified
			dnl since the last caching, assume it to be
			dnl invalid.
			last_modified="`ls -Lt $configure_cache configure`"
			case "x$last_modified" in
				xconfigure*)
					AC_MSG_RESULT([found but out of date])
					rm -f $configure_cache
					;;
			esac
		fi

		dnl if the cache still exists, load it
		if test -f "$configure_cache" ; then
			dnl go ahead and load the cache
			AC_MSG_RESULT([found $configure_cache])
			case $configure_cache in
				[\\/]* | ?:[\\/]* )
					. $configure_cache
					;;
				*)
					. ./$configure_cache
					;;
			esac
		fi
	else
		AC_MSG_RESULT([not found])
	fi

	dnl if we are on sgi, bash may choke on bad sed syntax in the cache
	if test "x$host_os" != "xirix6.5" ; then
		AC_MSG_NOTICE([*** Automatically caching to $configure_cache ***])
		>$configure_cache
		cache_file="$configure_cache"
		CONFIG_CACHE="$cache_file"
	else
		AC_MSG_NOTICE([Automatic caching is unavailable on IRIX])
	fi
	AC_SUBST(CONFIG_CACHE)
else
	AC_MSG_RESULT($cache_file)
fi
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
