SET(flex_SOURCES
	ccl.c
	dfa.c
	ecs.c
	scanflags.c
	gen.c
	main.c
	misc.c
	nfa.c
	sym.c
	tblcmp.c
	yylex.c
	options.c
	scanopt.c
	buf.c
	tables.c
	tables_shared.c
	filter.c
	regex.c
)
SET(flex_bootstrap_SOURCES
	scan.c   # scan.l
	parse.c  # parse.y
	skel.c   # flex.skl
)

if(NOT WIN32)
	find_library(M_LIBRARY NAMES m PATHS /usr/lib /usr/local/lib)
ELSE(NOT WIN32)
	set(M_LIBRARY "")
endif(NOT WIN32)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	)

add_definitions(
	-DHAVE_CONFIG_H
	)

add_executable(flex ${flex_SOURCES} ${flex_bootstrap_SOURCES})
if(M_LIBRARY)
	target_link_libraries(flex ${M_LIBRARY})
endif(M_LIBRARY)
