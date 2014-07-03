include_directories(
	${UTAHRLE_INCLUDE_DIR}
	${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/rletogif
)

# Disabled due to various external dependancies:
#	iristorle
#	pgmtorle
#	ppmtorle
#	rastorle
#	rletocgm
#	rletoiris
#	rletoppm
#	rletorast
#	rletotiff
#	tifftorle

URT_EXEC(aliastorle aliastorle.c)
URT_EXEC(cubitorle cubitorle.c)
URT_EXEC(giftorle giftorle.c)
URT_EXEC(graytorle graytorle.c)
# URT_EXEC(iristorle iristorle.c)
URT_EXEC(painttorle painttorle.c)
# URT_EXEC(pgmtorle pgmtorle.c)
# URT_EXEC(ppmtorle ppmtorle.c)
# URT_EXEC(rastorle rastorle.c)
URT_EXEC(rawtorle rawtorle.c)
URT_EXEC(rletoabA60 rletoabA60.c)
URT_EXEC(rletoalias rletoalias.c)
URT_EXEC(rletoascii rletoascii.c)
# URT_EXEC(rletocgm rletocgm.c)
URT_EXEC(rletogray rletogray.c)
# URT_EXEC(rletoiris rletoiris.c)
URT_EXEC(rletopaint rletopaint.c)
# URT_EXEC(rletoppm rletoppm.c)
URT_EXEC(rletops rletops.c)
# URT_EXEC(rletorast rletorast.c)
URT_EXEC(rletoraw rletoraw.c)
URT_EXEC(rletorla rletorla.c)
URT_EXEC(rletotarga rletotarga.c)
# URT_EXEC(rletotiff rletotiff.c)
URT_EXEC(rletovcr rletovcr.c)
URT_EXEC(targatorle targatorle.c)
# URT_EXEC(tifftorle tifftorle.c)
URT_EXEC(vcrtorle vcrtorle.c)
URT_EXEC(wasatchrle wasatchrle.c)
URT_EXEC(xbmtorle xbmtorle.c)

URT_EXEC(rletoabA62 "rletoabA62/rle.c rletoabA62/rletoabA62.c")
URT_EXEC(rletogif "rletogif/compgif.c rletogif/gifencod.c rletogif/rletogif.c")
