set(GLPK_SOURCES
glpavl.c
glpbcs1.c
glpbcs2.c
glpchol.c
glpdmp.c
glphbsm.c
glpies1.c
glpies2.c
glpies3.c
glpinv.c
glpipm.c
glplib1.c
glplib2.c
glplib3.c
glplpp1.c
glplpp2.c
glplpt.c
glplpx1.c
glplpx2.c
glplpx3.c
glplpx4.c
glplpx5.c
glplpx6a.c
glplpx6b.c
glplpx6c.c
glplpx7.c
glplpx8a.c
glplpx8c.c
glplpx8d.c
glpluf.c
glpmat.c
glpmip1.c
glpmpl1.c
glpmpl2.c
glpmpl3.c
glpmpl4.c
glpmps.c
glpqmd.c
glpspm.c
glpspx1.c
glpspx2.c
glpstr.c
glptsp.c
)

file(GLOB GLPK_HEADERS *.h)

include_directories(
    ../../../include
)

add_definitions(
   -DON_DLL_EXPORTS
)

if(MSVC)
    add_definitions(
        -DUNICODE
    )

endif(MSVC)

add_library(glpk SHARED ${GLPK_SOURCES})

target_link_libraries(glpk ${GLPK_LINKLIBRARIES})

set_property(TARGET glpk PROPERTY LINK_INTERFACE_LIBRARIES "")

install(TARGETS glpk 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES ${GLPK_HEADERS} DESTINATION include)
set(GLPK_SOURCES
glpavl.c
glpbcs1.c
glpbcs2.c
glpchol.c
glpdmp.c
glphbsm.c
glpies1.c
glpies2.c
glpies3.c
glpinv.c
glpipm.c
glplib1.c
glplib2.c
glplib3.c
glplpp1.c
glplpp2.c
glplpt.c
glplpx1.c
glplpx2.c
glplpx3.c
glplpx4.c
glplpx5.c
glplpx6a.c
glplpx6b.c
glplpx6c.c
glplpx7.c
glplpx8a.c
glplpx8c.c
glplpx8d.c
glpluf.c
glpmat.c
glpmip1.c
glpmpl1.c
glpmpl2.c
glpmpl3.c
glpmpl4.c
glpmps.c
glpqmd.c
glpspm.c
glpspx1.c
glpspx2.c
glpstr.c
glptsp.c
)

file(GLOB GLPK_HEADERS *.h)

include_directories(
    ../../../include
)

add_definitions(
   -DON_DLL_EXPORTS
)

if(MSVC)
    add_definitions(
        -DUNICODE
    )

endif(MSVC)

add_library(glpk SHARED ${GLPK_SOURCES})

target_link_libraries(glpk ${GLPK_LINKLIBRARIES})

set_property(TARGET glpk PROPERTY LINK_INTERFACE_LIBRARIES "")

install(TARGETS glpk 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES ${GLPK_HEADERS} DESTINATION include)
