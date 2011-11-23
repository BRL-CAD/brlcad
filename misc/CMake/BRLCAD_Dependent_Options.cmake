#  B R L C A D _ D E P E N D E N T _ O P T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
# Macro to provide an option only if a set of other variables are ON.
# Based on VTK's VTK_DEPENDENT_OPTION macro.
#
# Example invocation:
#
#  BRLCAD_DEPENDENT_OPTION(USE_FOO "Use Foo" ON "USE_BAR;USE_ZOT" OFF)
#
# If both USE_BAR and USE_ZOT are true, this provides an option called
# USE_FOO that defaults to ON.  Otherwise, it sets USE_FOO to OFF.  If
# the status of USE_BAR or USE_ZOT ever changes, any value for the
# USE_FOO option is saved so that when the option is re-enabled it
# retains its old value.


MACRO(BRLCAD_DEPENDENT_OPTION option doc default depends force)
  IF(${option}_ISSET MATCHES "^${option}_ISSET$")
    SET(${option}_AVAILABLE 1)
    FOREACH(d ${depends})
      IF(NOT ${d})
        SET(${option}_AVAILABLE 0)
      ENDIF(NOT ${d})
    ENDFOREACH(d)

    IF(${option}_AVAILABLE)
      OPTION(${option} "${doc}" "${default}")
      SET(${option} "${${option}}" CACHE BOOL "${doc}" FORCE)
    ELSE(${option}_AVAILABLE)
      IF(NOT ${option} MATCHES "^${option}$")
        SET(${option} "${${option}}" CACHE INTERNAL "${doc}")
      ENDIF(NOT ${option} MATCHES "^${option}$")
      SET(${option} ${force})
    ENDIF(${option}_AVAILABLE)

  ELSE(${option}_ISSET MATCHES "^${option}_ISSET$")
    SET(${option} "${${option}_ISSET}")
  ENDIF(${option}_ISSET MATCHES "^${option}_ISSET$")
ENDMACRO(BRLCAD_DEPENDENT_OPTION)


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
