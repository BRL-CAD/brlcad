include_directories(
	${TCL_INCLUDE_PATH}
)

add_definitions(
    -DRT_MULTISPECTRAL
)

BRLCAD_ADDLIB(libmultispectral "init.c sh_temp.c" "liboptical librt librtms-static")
SET_TARGET_PROPERTIES(libmultispectral PROPERTIES VERSION 19.0.1 SOVERSION 19)
