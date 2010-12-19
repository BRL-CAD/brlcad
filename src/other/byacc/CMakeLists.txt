# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(BYACC)


SET(BYACC_SRCS
	closure.c
	error.c
	graph.c
	lalr.c
	lr0.c
	main.c
	mkpar.c
	output.c
	reader.c
	skeleton.c
	symtab.c
	verbose.c
	warshall.c
	)

ADD_EXECUTABLE(byacc ${BYACC_SRCS})
