# Include directories needed by libfft users
set(FFT_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

# local directories
set(FFT_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(fft FFT_INCLUDE_DIRS FFT_LOCAL_INCLUDE_DIRS)

add_executable(fftc fftc.c splitditc.c)
set_property(TARGET fftc APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
SetTargetFolder(fftc "Compilation Utilities")
target_link_libraries(fftc ${M_LIBRARY})
CMAKEFILES(fftc.c splitditc.c)

add_executable(ifftc ifftc.c ditsplitc.c)
set_property(TARGET ifftc APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
SetTargetFolder(ifftc "Compilation Utilities")
target_link_libraries(ifftc ${M_LIBRARY})
CMAKEFILES(ifftc.c ditsplitc.c)

# We need to generate some of libfft's source files
function(fft_gen numlist targetdir outfiles)
  file(MAKE_DIRECTORY "${targetdir}")
  foreach(filenum ${numlist})
    add_custom_command(
      OUTPUT ${targetdir}/rfft${filenum}.c
      COMMAND fftc ${filenum} > ${targetdir}/rfft${filenum}.c
      DEPENDS fftc
      )
    set(output_files "${output_files};${targetdir}/rfft${filenum}.c")
    add_custom_command(
      OUTPUT ${targetdir}/irfft${filenum}.c
      COMMAND ifftc ${filenum} > ${targetdir}/irfft${filenum}.c
      DEPENDS ifftc
      )
    set(output_files "${output_files};${targetdir}/irfft${filenum}.c")
  endforeach(filenum ${FFT_NUMLIST})
  set(${outfiles} "${output_files}" PARENT_SCOPE)
endfunction(fft_gen numlist targetdir outfiles)

# 512 and 1024 are tough in optimized compiles, don't
# go that high until there's a demonstrated need. Only
# use in BRL-CAD right now is 256.
set(FFT_NUMLIST "16;32;64;128;256")

set(LIBFFT_SRCS
  fftfast.c
  splitdit.c
  ditsplit.c
  )
fft_gen("${FFT_NUMLIST}" "${CMAKE_CURRENT_BINARY_DIR}/shared" FFT_GEN_SHARED_SRCS)
fft_gen("${FFT_NUMLIST}" "${CMAKE_CURRENT_BINARY_DIR}/static" FFT_GEN_STATIC_SRCS)
BRLCAD_ADDLIB(libfft "${LIBFFT_SRCS}" "${M_LIBRARY}" SHARED_SRCS "${FFT_GEN_SHARED_SRCS}" STATIC_SRCS "${FFT_GEN_STATIC_SRCS}")

set_target_properties(libfft PROPERTIES LINKER_LANGUAGE C)
set_target_properties(libfft PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_executable(fftest fftest.c)
set_property(TARGET fftest APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
SetTargetFolder(fftest "Compilation Utilities")
target_link_libraries(fftest libfft ${M_LIBRARY})
CMAKEFILES(fftest.c)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
