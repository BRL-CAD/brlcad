# Automate putting variables from tests into a config.h.in file

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckTypeSize)

MACRO(BRLCAD_FUNCTION_EXISTS function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_FUNCTION_EXISTS)

MACRO(BRLCAD_INCLUDE_FILE filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_INCLUDE_FILE)

MACRO(BRLCAD_TYPE_SIZE typename var)
  CHECK_TYPE_SIZE(${typename} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_TYPE_SIZE)

