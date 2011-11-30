# Include directories needed by libfft users
SET(FFT_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)

# local directories
SET(FFT_LOCAL_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}
)

BRLCAD_INCLUDE_DIRS(FFT_INCLUDE_DIRS)

ADD_EXECUTABLE(fftc fftc.c splitditc.c)
target_link_libraries(fftc ${M_LIBRARY})

ADD_EXECUTABLE(ifftc ifftc.c ditsplitc.c)
target_link_libraries(ifftc ${M_LIBRARY})

# 512 and 1024 are tough in optimized compiles, don't
# go that high until there's a demonstrated need. Only
# use in BRL-CAD right now is 256.
SET(FFT_NUMLIST "16;32;64;128;256")
SET(FFT_GEN_SRCS "")

FOREACH(filenum ${FFT_NUMLIST})
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rfft${filenum}.c
    COMMAND fftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/rfft${filenum}.c
    DEPENDS fftc
  )
  SET(FFT_GEN_SRCS "${FFT_GEN_SRCS};${CMAKE_CURRENT_BINARY_DIR}/rfft${filenum}.c")
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/irfft${filenum}.c
    COMMAND ifftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/irfft${filenum}.c
    DEPENDS ifftc
  )
  SET(FFT_GEN_SRCS "${FFT_GEN_SRCS};${CMAKE_CURRENT_BINARY_DIR}/irfft${filenum}.c")
ENDFOREACH(filenum ${FFT_NUMLIST})

SET(LIBFFT_SRCS
  fftfast.c
  splitdit.c
  ditsplit.c
  ${FFT_GEN_SRCS}
)

BRLCAD_ADDLIB(libfft "${LIBFFT_SRCS}" "${M_LIBRARY}")
SET_TARGET_PROPERTIES(libfft PROPERTIES LINKER_LANGUAGE C)
SET_TARGET_PROPERTIES(libfft PROPERTIES VERSION 19.0.1 SOVERSION 19)

add_executable(fftest fftest.c)
target_link_libraries(fftest libfft ${M_LIBRARY})
CMAKEFILES(Makefile.am)
