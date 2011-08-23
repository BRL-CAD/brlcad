# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(LEMON)

add_executable(lemon lemon.c)
configure_file(lempar.c ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lempar.c COPY_ONLY)
