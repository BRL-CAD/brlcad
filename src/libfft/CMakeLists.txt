include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_SOURCE_DIR}/src/libfft
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

ADD_EXECUTABLE(fftc fftc.c splitditc.c)
target_link_libraries(fftc ${M_LIBRARY})

ADD_EXECUTABLE(ifftc ifftc.c ditsplitc.c)
target_link_libraries(ifftc ${M_LIBRARY})


SET(FFT_NUMLIST "16;32;64;128;256;512;1024")
SET(FFT_GEN_SRCS "")

FOREACH(filenum ${FFT_NUMLIST})
  ADD_CUSTOM_COMMAND(
    OUTPUT rfft${filenum}.c
    COMMAND fftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/rfft${filenum}.c
    DEPENDS fftc
  )
  SET(FFT_GEN_SRCS "${FFT_GEN_SRCS};rfft${filenum}.c")
  ADD_CUSTOM_COMMAND(
    OUTPUT irfft${filenum}.c
    COMMAND ifftc ${filenum} > ${CMAKE_CURRENT_BINARY_DIR}/irfft${filenum}.c
    DEPENDS ifftc
  )
  SET(FFT_GEN_SRCS "${FFT_GEN_SRCS};irfft${filenum}.c")
ENDFOREACH(filenum ${FFT_NUMLIST})

SET(LIBFFT_SRCS
  fftfast.c
  splitdit.c
  ditsplit.c
  ${FFT_GEN_SRCS}
)

BRLCAD_ADDLIB(fft "${LIBFFT_SRCS}" "NONE")
SET_TARGET_PROPERTIES(fft PROPERTIES LINKER_LANGUAGE C)
SET_TARGET_PROPERTIES(fft PROPERTIES VERSION 19.0.1 SOVERSION 19)

add_executable(fftest fftest.c)
target_link_libraries(fftest fft ${M_LIBRARY})
