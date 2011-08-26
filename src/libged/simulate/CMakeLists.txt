set(SIMULATE_SOURCES
    simphysics.cpp
    simulate.c   
)

# Bullet physics library : includes and libs
IF(BULLET_FOUND)
    include_directories(${BULLET_INCLUDE_DIR})
    BRLCAD_ADDLIB(simulate "${SIMULATE_SOURCES}" ${BULLET_LIBRARIES})
    SET_TARGET_PROPERTIES(simulate PROPERTIES VERSION 19.0.1 SOVERSION 19)    
ENDIF(BULLET_FOUND)


