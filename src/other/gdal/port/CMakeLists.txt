set(cpl_srcs
	cpl_conv.cpp
	cpl_error.cpp
	cpl_string.cpp
	cplgetsymbol.cpp
	cplstringlist.cpp
	cpl_strtod.cpp
	cpl_path.cpp
	cpl_csv.cpp
	cpl_findfile.cpp
	cpl_minixml.cpp
	cpl_multiproc.cpp
	cpl_list.cpp
	cpl_getexecpath.cpp
	cplstring.cpp
	cpl_vsil_win32.cpp
	cpl_vsisimple.cpp
	cpl_vsil.cpp
	cpl_vsi_mem.cpp
	cpl_vsil_unix_stdio_64.cpp
	cpl_http.cpp
	cpl_hash_set.cpp
	cplkeywordparser.cpp
	cpl_recode.cpp
	cpl_recode_iconv.cpp
	cpl_recode_stub.cpp
	cpl_quad_tree.cpp
	cpl_atomic_ops.cpp
	cpl_vsil_subfile.cpp
	cpl_time.cpp
	cpl_vsil_stdout.cpp
	cpl_vsil_sparsefile.cpp
	cpl_vsil_abstract_archive.cpp
	cpl_vsil_tar.cpp
	cpl_vsil_stdin.cpp
	cpl_vsil_buffered_reader.cpp
	cpl_base64.cpp
	cpl_vsil_curl.cpp
	cpl_vsil_curl_streaming.cpp
	cpl_vsil_cache.cpp
	cpl_xml_validate.cpp
	cpl_spawn.cpp
	cpl_google_oauth2.cpp
	cpl_progress.cpp
	cpl_virtualmem.cpp
	cpl_worker_thread_pool.cpp
	cpl_vsil_crypt.cpp
	cpl_sha1.cpp
	cpl_sha256.cpp
	cpl_aws.cpp
	cpl_vsi_error.cpp
	cpl_cpu_features.cpp
	cpl_google_cloud.cpp
	)

if(ZLIB_LIBRARY)
	set(cpl_srcs ${cpl_srcs}
		cpl_vsil_gzip.cpp
		cpl_minizip_ioapi.cpp
		cpl_minizip_unzip.cpp
		cpl_minizip_zip.cpp
		)
endif(ZLIB_LIBRARY)


add_library(gdal_cpl_obj OBJECT ${cpl_srcs})
set_target_properties(gdal_cpl_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")

