set(VRML_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )

include_directories(${VRML_INCLUDE_DIRS})

set(vrml_SRCS
  node.cpp
  node_type.cpp
  parser.cpp 
  file_util.cpp 
  string_util.cpp
  transform_node.cpp  
  vrml-g.cpp
  )
  
BRLCAD_ADDEXEC(vrml-g "${vrml_SRCS}" "libwdb;librt;libbu")


CMAKEFILES(
  file_util.h
  node.h
  node_type.h
  parser.h
  string_util.h
  transform_node.h
  )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
