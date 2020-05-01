include_directories(
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

project(fuzz)

add_executable(fuzz_test fuzz_ged.cpp)
target_compile_options(fuzz_test
            PRIVATE $<$<C_COMPILER_ID:Clang>:-g -O1 -fsanitize=fuzzer>
            )

target_link_libraries(fuzz_test PUBLIC libbu librt libged
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
