set(LIBGED_SIM_SOURCES
    simphysics.cpp
    simulate.c   
)

# Bullet physics library : includes and libs
IF(BULLET_FOUND)
    include_directories(${BULLET_INCLUDE_DIR})
    BRLCAD_ADDLIB(libgedsim "${LIBGED_SIM_SOURCES}" ${BULLET_LIBRARIES})
    SET_TARGET_PROPERTIES(libgedsim PROPERTIES VERSION 19.0.1 SOVERSION 19)
    SET(GEDSIM_LIB libgedsim)
ENDIF(BULLET_FOUND)


