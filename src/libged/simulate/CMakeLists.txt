set(LIBGED_SIM_SOURCES
    simphysics.cpp
    simulate.c 
    simulate.h  
)

# Only enable simulate if the Bullet physics library includes and libs are
# present.
IF(BULLET_FOUND)
	GED_ADD_CMD(LIBGED_SIM_SOURCES BULLET_INCLUDE_DIR BULLET_LIBRARIES)
ENDIF(BULLET_FOUND)


