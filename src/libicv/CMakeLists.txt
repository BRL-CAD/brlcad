brlcad_find_package(NETPBM REQUIRED)
brlcad_find_package(PNG REQUIRED)
brlcad_find_package(JPEG REQUIRED)

if(TARGET JPEG::JPEG)
  if(JPEG_INCLUDE_DIRS)
    set_property(TARGET JPEG::JPEG APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIRS}")
  endif()
  if(JPEG_LIBRARY_RELEASE)
    set_property(TARGET JPEG::JPEG APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION_RELEASE "${JPEG_LIBRARY_RELEASE}")
  endif()
  if(JPEG_LIBRARY_DEBUG)
    set_property(TARGET JPEG::JPEG APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION_DEBUG "${JPEG_LIBRARY_DEBUG}")
  endif()
  if(JPEG_LIBRARY)
    set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION "${JPEG_LIBRARY}")
  elseif(JPEG_LIBRARY_RELEASE)
    set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION "${JPEG_LIBRARY_RELEASE}")
  elseif(JPEG_LIBRARY_DEBUG)
    set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION "${JPEG_LIBRARY_DEBUG}")
  endif()
endif()

set(
  LIBICV_SOURCES
  PImgHash.cpp
  asciiart.cpp
  bw.c
  color_space.c
  crop.c
  diff.cpp
  dpix.c
  encoding.c
  fileformat.c
  filter.c
  jpeg.c
  operations.c
  pdiff.cpp
  pix.c
  png.cpp
  ppm.c
  rle.cpp
  rot.c
  size.c
  stat.c
)

# Note - libicv_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(ICV_LIBS ${libicv_deps} PNG::PNG NETPBM::NETPBM JPEG::JPEG)
set(LOCAL_ICV_INCLUDE_DIRS ${PNG_INCLUDE_DIRS} ${NETPBM_INCLUDE_DIR} ${JPEG_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

brlcad_addlib(libicv "${LIBICV_SOURCES}" "" "${LOCAL_ICV_INCLUDE_DIRS}"
  PRIVATE_LIBS ${ICV_LIBS}
)
set_target_properties(libicv PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libicv APPEND PROPERTY COMPILE_DEFINITIONS NETPBM_DLL_IMPORTS)
  # Anyone who needs to import libicv will also need to import the library defines
  set_property(TARGET libicv APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS NETPBM_DLL_IMPORTS)
endif(HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  TODO
  PImgHash.h
  pixcii.hpp
  icv_private.h
  rle.hpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
