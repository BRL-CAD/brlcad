include_directories(
	${PNG_INCLUDE_DIR}
       	${ZLIB_INCLUDE_DIR}
       	)

set(OSG_OSG_PLUGIN_SRCS
  ReaderWriterPNG.cpp
  )

if(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_png MODULE ${OSG_OSG_PLUGIN_SRCS})
else(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_png STATIC ${OSG_OSG_PLUGIN_SRCS})
endif(DYNAMIC_OPENSCENEGRAPH)

target_link_libraries(osgdb_png osgDB osgUtil osg OpenThreads
  ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY} ${PNG_LIBRARY} ${ZLIB_LIBRARY}
  )
install(TARGETS osgdb_png
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

IF(MSVC)
    SET_TARGET_PROPERTIES(osgdb_png PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:MSVCRT")
ENDIF()


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
