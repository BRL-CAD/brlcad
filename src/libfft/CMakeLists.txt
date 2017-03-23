# Include directories needed by libfft users
set(FFT_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

# local directories
set(FFT_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(fft FFT_INCLUDE_DIRS FFT_LOCAL_INCLUDE_DIRS)

add_executable(fftc fftc.c splitditc.c)
SetTargetFolder(fftc "Compilation Utilities")
target_link_libraries(fftc ${M_LIBRARY})

add_executable(ifftc ifftc.c ditsplitc.c)
SetTargetFolder(ifftc "Compilation Utilities")
target_link_libraries(ifftc ${M_LIBRARY})

# 512 and 1024 are tough in optimized compiles, don't
# go that high until there's a demonstrated need. Only
# use in BRL-CAD right now is 256.
set(FFT_NUMLIST "16;32;64;128;256")
set(FFT_GEN_SO_SRCS "")
set(FFT_GEN_STATIC_SRCS "")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shared")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/static")

foreach(filenum ${FFT_NUMLIST})
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shared/rfft${filenum}.c
    COMMAND fftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/shared/rfft${filenum}.c
    DEPENDS fftc
    )
  set(FFT_GEN_SO_SRCS "${FFT_GEN_SO_SRCS};${CMAKE_CURRENT_BINARY_DIR}/shared/rfft${filenum}.c")
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shared/irfft${filenum}.c
    COMMAND ifftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/shared/irfft${filenum}.c
    DEPENDS ifftc
    )
  set(FFT_GEN_SO_SRCS "${FFT_GEN_SO_SRCS};${CMAKE_CURRENT_BINARY_DIR}/shared/irfft${filenum}.c")
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/static/rfft${filenum}.c
    COMMAND fftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/static/rfft${filenum}.c
    DEPENDS fftc
    )
  set(FFT_GEN_STATIC_SRCS "${FFT_GEN_STATIC_SRCS};${CMAKE_CURRENT_BINARY_DIR}/static/rfft${filenum}.c")
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/static/irfft${filenum}.c
    COMMAND ifftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/static/irfft${filenum}.c
    DEPENDS ifftc
    )
  set(FFT_GEN_STATIC_SRCS "${FFT_GEN_STATIC_SRCS};${CMAKE_CURRENT_BINARY_DIR}/static/irfft${filenum}.c")
endforeach(filenum ${FFT_NUMLIST})

set(LIBFFT_SRCS
  fftfast.c
  splitdit.c
  ditsplit.c
  )

BRLCAD_ADDLIB(libfft "${LIBFFT_SRCS}" "${M_LIBRARY}" SO_SRCS "${FFT_GEN_SO_SRCS}" STATIC_SRCS "${FFT_GEN_STATIC_SRCS}")
set_target_properties(libfft PROPERTIES LINKER_LANGUAGE C)
set_target_properties(libfft PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_executable(fftest fftest.c)
SetTargetFolder(fftest "Compilation Utilities")
target_link_libraries(fftest libfft ${M_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
