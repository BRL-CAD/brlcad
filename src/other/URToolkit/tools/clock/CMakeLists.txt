include_directories(
	${UTAHRLE_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	)

IF(SH_EXEC)

	#ADD_CUSTOM_COMMAND(
	#		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
	#		COMMAND ${SH_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/makeFont ${CMAKE_CURRENT_SOURCE_DIR}/font.src > ${CMAKE_CURRENT_BINARY_DIR}/font.c
	#		)

	#URT_EXEC(rleClock "rleClock.c ${CMAKE_CURRENT_BINARY_DIR}/font.c")

	#The font file is supposed to be generated, and the above code will do so, but it appears the makeFont
	#script has some sort of issue, at least on OSX - font.c has been checked in to the repository.  Go with that for now.	
	URT_EXEC(rleClock "rleClock.c font.c")

ELSE(SH_EXEC)

	URT_EXEC(rleClock "rleClock.c font.c")

ENDIF(SH_EXEC)

