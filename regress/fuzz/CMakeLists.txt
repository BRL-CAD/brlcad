include_directories(${BU_INCLUDE_DIRS}, ${CMAKE_CURRENT_SOURCE_DIR}/../../build/include/openNURBS "${ZLIB_INCLUDE_DIRS}"
    "${REGEX_INCLUDE_DIRS}")


# Not sure what this value should be
#cmake_minimum_required (VERSION 3.10)

# SETTING COMPILER DOES NOT WORK -- NEED TO COMPILE WITH CLANG
#set(CMAKE_C_COMPILER "/usr/bin/clang")
#set(CMAKE_CXX_COMPLER "/usr/bin/clang++")

project(fuzz)


add_executable(fuzz_test fuzz_ged.cpp)
target_compile_options(fuzz_test
            PRIVATE $<$<C_COMPILER_ID:Clang>:-g -O1 -fsanitize=fuzzer>
            )

target_link_libraries(fuzz_test PUBLIC libbu librt
            PRIVATE $<$<C_COMPILER_ID:Clang>:-fsanitize=fuzzer>
            )


configure_file(run.sh "${CMAKE_CURRENT_BINARY_DIR}/fuzz" COPYONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/fuzz" DESTINATION ${BIN_DIR})
configure_file(run.sh ${CMAKE_BINARY_DIR}/bin/fuzz COPYONLY)

#add_test(NAME fuzz COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run)

add_custom_target(fuzz
  COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run
  )


#Not sure what this is for
#CMAKEFILES(CMakeLists.txt)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
