
set(OGRSF_FMTS
	aeronavfaa arcgen	avc
	bna
	csv
	dgn dxf
	edigeo
	generic geoconcept geojson	gmt gpsbabel gtm
	htf
	idrisi
	mem mitab
	ntf
	openair openfilegdb
	pds pgdump
	rec
	s57 segukooa segy selafin shape sua sxf
	tiger
	vdv vrt
	wasp
	xplane
)


set(ogrsf_fmt_srcs)
foreach(frmt ${OGRSF_FMTS})
	file(GLOB_RECURSE ${frmt}_srcs LIST_DIRECTORIES false RELATIVE ${GDAL_SOURCE_DIR} ogr/ogrsf_frmts/${frmt}/*.c*)
	include_directories(ogr/ogrsf_frmts/${frmt})
	message("${frmt}_srcs: ${${frmt}_srcs}")
	set(ogrsf_fmt_srcs ${ogrsf_fmt_srcs} ${${frmt}_srcs})
endforeach(frmt ${OGRSF_FMTS})

