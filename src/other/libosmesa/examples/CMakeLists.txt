include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_executable(osdemo osdemo.c)
target_link_libraries(osdemo osmesa ${M_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
set_property(TARGET osdemo APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_IMPORTS")

find_package(SDL)

#if (SDL_FOUND)
#  add_executable(texobj texobj.c)
#  target_link_libraries(texobj osmesa ${SDL_LIBRARY} ${M_LIBRARY})
#  set_property(TARGET texobj APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_IMPORTS")
#  add_executable(gears gears.c)
#  target_link_libraries(gears osmesa ${SDL_LIBRARY} ${M_LIBRARY})
#  set_property(TARGET gears APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_IMPORTS")
#  add_executable(triangle triangle.c)
#  target_link_libraries(triangle osmesa ${SDL_LIBRARY} ${M_LIBRARY})
#  set_property(TARGET triangle APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_IMPORTS")
#endif (SDL_FOUND)

#find_package(TCL)
#if (TCL_FOUND)
#  include_directories(
#    ${TCL_INCLUDE_PATH}
#    )
#  add_executable(tcltest tcl.cpp)
#  target_link_libraries(tcltest osmesa ${TCL_LIBRARY} ${TK_LIBRARY} ${M_LIBRARY})
#  set_property(TARGET tcltest APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_IMPORTS")
#endif (TCL_FOUND)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
