include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	)

set(libtiff_srcs
	tif_aux.c
	tif_close.c
	tif_codec.c
	tif_color.c
	tif_compress.c
	tif_dir.c
	tif_dirinfo.c
	tif_dirread.c
	tif_dirwrite.c
	tif_dumpmode.c
	tif_error.c
	tif_extension.c
	tif_fax3.c
	tif_fax3sm.c
	tif_getimage.c
	tif_jpeg.c
	tif_jpeg_12.c
	tif_flush.c
	tif_luv.c
	tif_lzw.c
	tif_next.c
	tif_ojpeg.c
	tif_open.c
	tif_packbits.c
	tif_pixarlog.c
	tif_predict.c
	tif_print.c
	tif_read.c
	tif_swab.c
	tif_strip.c
	tif_thunder.c
	tif_tile.c
	tif_vsi.c
	tif_version.c
	tif_warning.c
	tif_write.c
	tif_zip.c
	tif_lzma.c
	)

add_definitions(-DDONT_DEPRECATE_SPRINTF)
if(ZLIB_LIBRARY)
	add_definitions(-DPIXARLOG_SUPPORT)
	add_definitions(-DZIP_SUPPORT)
endif(ZLIB_LIBRARY)

add_library(libtiff_obj OBJECT ${libtiff_srcs})
set_target_properties(libtiff_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
