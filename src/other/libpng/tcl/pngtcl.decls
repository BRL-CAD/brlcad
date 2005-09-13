# pngtcl.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the PNGTCL library via the stubs table. This file
# is used to generate the pngtclDecls.h/pngtclStubsLib.c/pngtclStubsInit.c
# files.
#	

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library pngtcl

# Define the PNGTCL interface:

interface pngtcl
#hooks {}

#########################################################################
###  PNG API

declare 0 generic {
    png_uint_32 png_access_version_number (void)
}
declare 1 generic {
    void png_set_sig_bytes (png_structp png_ptr, int num_bytes)
}
declare 2 generic {
    int png_sig_cmp (png_bytep sig, png_size_t start, png_size_t num_to_check)
}
declare 3 generic {
    int png_check_sig (png_bytep sig, int num)
}
declare 4 generic {
    png_structp png_create_read_struct (png_const_charp user_png_ver,
    	png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)
}
declare 5 generic {
    png_structp png_create_write_struct (png_const_charp user_png_ver,
    	png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)
}
declare 6 generic {
    png_uint_32 png_get_compression_buffer_size (png_structp png_ptr)
}
declare 7 generic {
    void png_set_compression_buffer_size (png_structp png_ptr, png_uint_32 size)
}
declare 8 generic {
    int png_reset_zstream (png_structp png_ptr)
}
declare 9 generic {!PNG_USER_MEM_SUPPORTED} {
    png_structp png_create_read_struct_2 (png_const_charp user_png_ver,
    	png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn,
    	png_voidp mem_ptr, png_malloc_ptr malloc_fn, png_free_ptr free_fn)
}
declare 10 generic {!PNG_USER_MEM_SUPPORTED} {
    png_structp png_create_write_struct_2 (png_const_charp user_png_ver,
    	png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn,
    	png_voidp mem_ptr, png_malloc_ptr malloc_fn, png_free_ptr free_fn)
}
declare 11 generic {
    void png_write_chunk (png_structp png_ptr, png_bytep chunk_name,
    	png_bytep data, png_size_t length)
}
declare 12 generic {
    void png_write_chunk_start (png_structp png_ptr, png_bytep chunk_name,
    	png_uint_32 length)
}
declare 13 generic {
    void png_write_chunk_data (png_structp png_ptr, png_bytep data,
    	png_size_t length)
}
declare 14 generic {
    void png_write_chunk_end (png_structp png_ptr)
}
declare 15 generic {
    png_infop png_create_info_struct (png_structp png_ptr)
}
declare 16 generic {
    void png_info_init (png_infop info_ptr)
}
declare 17 generic {
    void png_write_info_before_PLTE (png_structp png_ptr, png_infop info_ptr)
}
declare 18 generic {
    void png_write_info (png_structp png_ptr, png_infop info_ptr)
}
declare 19 generic {
    void png_read_info (png_structp png_ptr, png_infop info_ptr)
}
declare 20 generic {!PNG_TIME_RFC1123_SUPPORTED} {
    png_charp png_convert_to_rfc1123 (png_structp png_ptr, png_timep ptime)
}
declare 21 generic {_WIN32_WCE !PNG_WRITE_tIME_SUPPORTED} {
    void png_convert_from_struct_tm (png_timep ptime, struct tm FAR * ttime)
}
declare 22 generic {_WIN32_WCE !PNG_WRITE_tIME_SUPPORTED} {
    void png_convert_from_time_t (png_timep ptime, time_t ttime)
}
declare 23 generic {!PNG_READ_EXPAND_SUPPORTED} {
    void png_set_expand (png_structp png_ptr)
}
declare 24 generic {!PNG_READ_EXPAND_SUPPORTED} {
    void png_set_gray_1_2_4_to_8 (png_structp png_ptr)
}
declare 25 generic {!PNG_READ_EXPAND_SUPPORTED} {
    void png_set_palette_to_rgb (png_structp png_ptr)
}
declare 26 generic {!PNG_READ_EXPAND_SUPPORTED} {
    void png_set_tRNS_to_alpha (png_structp png_ptr)
}
declare 27 generic {{!PNG_READ_BGR_SUPPORTED !PNG_WRITE_BGR_SUPPORTED}} {
    void png_set_bgr (png_structp png_ptr)
}
declare 28 generic {!PNG_READ_GRAY_TO_RGB_SUPPORTED} {
    void png_set_gray_to_rgb (png_structp png_ptr)
}
declare 29 generic {!PNG_READ_RGB_TO_GRAY_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_rgb_to_gray (png_structp png_ptr, int error_action, double red, double green)
}
declare 30 generic {!PNG_READ_RGB_TO_GRAY_SUPPORTED} {
    void png_set_rgb_to_gray_fixed (png_structp png_ptr, int error_action,
    	png_fixed_point red, png_fixed_point green)
}
declare 31 generic {!PNG_READ_RGB_TO_GRAY_SUPPORTED} {
    png_byte png_get_rgb_to_gray_status (png_structp png_ptr)
}
declare 32 generic {
    void png_build_grayscale_palette (int bit_depth, png_colorp palette)
}
declare 33 generic {!PNG_READ_STRIP_ALPHA_SUPPORTED} {
    void png_set_strip_alpha (png_structp png_ptr)
}
declare 34 generic {{!PNG_READ_SWAP_ALPHA_SUPPORTED !PNG_WRITE_SWAP_ALPHA_SUPPORTED}} {
    void png_set_swap_alpha (png_structp png_ptr)
}
declare 35 generic {{!PNG_READ_INVERT_ALPHA_SUPPORTED !PNG_WRITE_INVERT_ALPHA_SUPPORTED}} {
    void png_set_invert_alpha (png_structp png_ptr)
}
declare 36 generic {{!PNG_READ_FILLER_SUPPORTED !PNG_WRITE_FILLER_SUPPORTED}} {
    void png_set_filler (png_structp png_ptr, png_uint_32 filler, int flags)
}
declare 37 generic {{!PNG_READ_SWAP_SUPPORTED !PNG_WRITE_SWAP_SUPPORTED}} {
    void png_set_swap (png_structp png_ptr)
}
declare 38 generic {{!PNG_READ_PACK_SUPPORTED !PNG_WRITE_PACK_SUPPORTED}} {
    void png_set_packing (png_structp png_ptr)
}
declare 39 generic {{!PNG_READ_PACKSWAP_SUPPORTED !PNG_WRITE_PACKSWAP_SUPPORTED}} {
    void png_set_packswap (png_structp png_ptr)
}
declare 40 generic {{!PNG_READ_SHIFT_SUPPORTED !PNG_WRITE_SHIFT_SUPPORTED}} {
    void png_set_shift (png_structp png_ptr, png_color_8p true_bits)
}
declare 41 generic {{!PNG_READ_INTERLACING_SUPPORTED !PNG_WRITE_INTERLACING_SUPPORTED}} {
    int png_set_interlace_handling (png_structp png_ptr)
}
declare 42 generic {{!PNG_READ_INVERT_SUPPORTED !PNG_WRITE_INVERT_SUPPORTED}} {
    void png_set_invert_mono (png_structp png_ptr)
}
declare 43 generic {!PNG_READ_BACKGROUND_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_background (png_structp png_ptr, png_color_16p background_color,
    	int background_gamma_code, int need_expand, double background_gamma)
}
declare 44 generic {!PNG_READ_16_TO_8_SUPPORTED} {
    void png_set_strip_16 (png_structp png_ptr)
}
declare 45 generic {!PNG_READ_DITHER_SUPPORTED} {
    void png_set_dither (png_structp png_ptr, png_colorp palette, int num_palette, int maximum_colors, png_uint_16p histogram, int full_dither)
}
declare 46 generic {!PNG_READ_GAMMA_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_gamma (png_structp png_ptr, double screen_gamma, double default_file_gamma)
}
declare 47 generic {{!PNG_READ_EMPTY_PLTE_SUPPORTED !PNG_WRITE_EMPTY_PLTE_SUPPORTED}} {
    void png_permit_empty_plte (png_structp png_ptr, int empty_plte_permitted)
}
declare 48 generic {!PNG_WRITE_FLUSH_SUPPORTED} {
    void png_set_flush (png_structp png_ptr, int nrows)
}
declare 49 generic {!PNG_WRITE_FLUSH_SUPPORTED} {
    void png_write_flush (png_structp png_ptr)
}
declare 50 generic {
    void png_start_read_image (png_structp png_ptr)
}
declare 51 generic {
    void png_read_update_info (png_structp png_ptr, png_infop info_ptr)
}
declare 52 generic {
    void png_read_rows (png_structp png_ptr, png_bytepp row, png_bytepp display_row, png_uint_32 num_rows)
}
declare 53 generic {
    void png_read_row (png_structp png_ptr, png_bytep row, png_bytep display_row)
}
declare 54 generic {
    void png_read_image (png_structp png_ptr, png_bytepp image)
}
declare 55 generic {
    void png_write_row (png_structp png_ptr, png_bytep row)
}
declare 56 generic {
    void png_write_rows (png_structp png_ptr, png_bytepp row, png_uint_32 num_rows)
}
declare 57 generic {
    void png_write_image (png_structp png_ptr, png_bytepp image)
}
declare 58 generic {
    void png_write_end (png_structp png_ptr, png_infop info_ptr)
}
declare 59 generic {
    void png_read_end (png_structp png_ptr, png_infop info_ptr)
}
declare 60 generic {
    void png_destroy_info_struct (png_structp png_ptr, png_infopp info_ptr_ptr)
}
declare 61 generic {
    void png_destroy_read_struct (png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
    	png_infopp end_info_ptr_ptr)
}
declare 62 generic {
    void png_read_destroy (png_structp png_ptr, png_infop info_ptr, png_infop end_info_ptr)
}
declare 63 generic {
    void png_destroy_write_struct (png_structpp png_ptr_ptr, png_infopp info_ptr_ptr)
}
if 0 {
    # Declared, not defined
    declare 64 generic {
	void png_write_destroy_info (png_infop info_ptr)
    }
}
declare 65 generic {
    void png_write_destroy (png_structp png_ptr)
}
declare 66 generic {
    void png_set_crc_action (png_structp png_ptr, int crit_action, int ancil_action)
}
declare 67 generic {
    void png_set_filter (png_structp png_ptr, int method, int filters)
}
declare 68 generic {!PNG_WRITE_WEIGHTED_FILTER_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_filter_heuristics (png_structp png_ptr, int heuristic_method,
    	int num_weights, png_doublep filter_weights, png_doublep filter_costs)
}
declare 69 generic {
    void png_set_compression_level (png_structp png_ptr, int level)
}
declare 70 generic {
    void png_set_compression_mem_level (png_structp png_ptr, int mem_level)
}
declare 71 generic {
    void png_set_compression_strategy (png_structp png_ptr, int strategy)
}
declare 72 generic {
    void png_set_compression_window_bits (png_structp png_ptr, int window_bits)
}
declare 73 generic {
    void png_set_compression_method (png_structp png_ptr, int method)
}
declare 74 generic {PNG_NO_STDIO} {
    void png_init_io (png_structp png_ptr, png_FILE_p fp)
}
declare 75 generic {
    void png_set_error_fn (png_structp png_ptr, png_voidp error_ptr,
    	png_error_ptr error_fn, png_error_ptr warning_fn)
}
declare 76 generic {
    png_voidp png_get_error_ptr (png_structp png_ptr)
}
declare 77 generic {
    void png_set_write_fn (png_structp png_ptr, png_voidp io_ptr,
    	png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn)
}
declare 78 generic {
    void png_set_read_fn (png_structp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn)
}
declare 79 generic {
    png_voidp png_get_io_ptr (png_structp png_ptr)
}
declare 80 generic {
    void png_set_read_status_fn (png_structp png_ptr, png_read_status_ptr read_row_fn)
}
declare 81 generic {
    void png_set_write_status_fn (png_structp png_ptr, png_write_status_ptr write_row_fn)
}
declare 82 generic {!PNG_USER_MEM_SUPPORTED} {
    void png_set_mem_fn (png_structp png_ptr, png_voidp mem_ptr,
    	png_malloc_ptr malloc_fn, png_free_ptr free_fn)
}
declare 83 generic {!PNG_USER_MEM_SUPPORTED} {
    png_voidp png_get_mem_ptr (png_structp png_ptr)
}
declare 84 generic {{!PNG_READ_USER_TRANSFORM_SUPPORTED !PNG_LEGACY_SUPPORTED}} {
    void png_set_read_user_transform_fn (png_structp png_ptr,
    	png_user_transform_ptr read_user_transform_fn)
}
declare 85 generic {{!PNG_WRITE_USER_TRANSFORM_SUPPORTED !PNG_LEGACY_SUPPORTED}} {
    void png_set_write_user_transform_fn (png_structp png_ptr,
    	png_user_transform_ptr write_user_transform_fn)
}
declare 86 generic {{!PNG_READ_USER_TRANSFORM_SUPPORTED !PNG_WRITE_USER_TRANSFORM_SUPPORTED !PNG_LEGACY_SUPPORTED}} {
    void png_set_user_transform_info (png_structp png_ptr, png_voidp user_transform_ptr,
    	int user_transform_depth, int user_transform_channels)
}
declare 87 generic {{!PNG_READ_USER_TRANSFORM_SUPPORTED !PNG_WRITE_USER_TRANSFORM_SUPPORTED !PNG_LEGACY_SUPPORTED}} {
    png_voidp png_get_user_transform_ptr (png_structp png_ptr)
}
declare 88 generic {!PNG_READ_USER_CHUNKS_SUPPORTED} {
    void png_set_read_user_chunk_fn (png_structp png_ptr, png_voidp user_chunk_ptr,
    	png_user_chunk_ptr read_user_chunk_fn)
}
declare 89 generic {!PNG_READ_USER_CHUNKS_SUPPORTED} {
    png_voidp png_get_user_chunk_ptr (png_structp png_ptr)
}
declare 90 generic {!PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_set_progressive_read_fn (png_structp png_ptr, png_voidp progressive_ptr,
    	png_progressive_info_ptr info_fn, png_progressive_row_ptr row_fn,
    	png_progressive_end_ptr end_fn)
}
declare 91 generic {!PNG_PROGRESSIVE_READ_SUPPORTED} {
    png_voidp png_get_progressive_ptr (png_structp png_ptr)
}
declare 92 generic {!PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_process_data (png_structp png_ptr, png_infop info_ptr,
    	png_bytep buffer, png_size_t buffer_size)
}
declare 93 generic {!PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_progressive_combine_row (png_structp png_ptr, png_bytep old_row,
    	png_bytep new_row)
}
declare 94 generic {
    png_voidp png_malloc (png_structp png_ptr, png_uint_32 size)
}
declare 95 generic {
    void png_free (png_structp png_ptr, png_voidp ptr)
}
declare 96 generic {
    void png_free_data (png_structp png_ptr, png_infop info_ptr, png_uint_32 free_me, int num)
}
declare 97 generic {!PNG_FREE_ME_SUPPORTED} {
    void png_data_freer (png_structp png_ptr, png_infop info_ptr, int freer, png_uint_32 mask)
}
declare 98 generic {!PNG_USER_MEM_SUPPORTED} {
    png_voidp png_malloc_default (png_structp png_ptr, png_uint_32 size)
}
declare 99 generic {!PNG_USER_MEM_SUPPORTED} {
    void png_free_default (png_structp png_ptr, png_voidp ptr)
}
declare 100 generic {
    png_voidp png_memcpy_check (png_structp png_ptr, png_voidp s1, png_voidp s2, png_uint_32 size)
}
declare 101 generic {
    png_voidp png_memset_check (png_structp png_ptr, png_voidp s1, int value, png_uint_32 size)
}
declare 102 generic {!USE_FAR_KEYWORD} {
    void *png_far_to_near (png_structp png_ptr,png_voidp ptr, int check)
}
declare 103 generic {
    void png_error (png_structp png_ptr, png_const_charp error)
}
declare 104 generic {
    void png_chunk_error (png_structp png_ptr, png_const_charp error)
}
declare 105 generic {
    void png_warning (png_structp png_ptr, png_const_charp message)
}
declare 106 generic {
    void png_chunk_warning (png_structp png_ptr, png_const_charp message)
}
declare 107 generic {
    png_uint_32 png_get_valid (png_structp png_ptr, png_infop info_ptr, png_uint_32 flag)
}
declare 108 generic {
    png_uint_32 png_get_rowbytes (png_structp png_ptr, png_infop info_ptr)
}
declare 109 generic {!PNG_INFO_IMAGE_SUPPORTED} {
    png_bytepp png_get_rows (png_structp png_ptr, png_infop info_ptr)
}
declare 110 generic {!PNG_INFO_IMAGE_SUPPORTED} {
    void png_set_rows (png_structp png_ptr, png_infop info_ptr, png_bytepp row_pointers)
}
declare 111 generic {
    png_byte png_get_channels (png_structp png_ptr, png_infop info_ptr)
}
declare 112 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_uint_32 png_get_image_width (png_structp png_ptr, png_infop info_ptr)
}
declare 113 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_uint_32 png_get_image_height (png_structp png_ptr, png_infop info_ptr)
}
declare 114 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_byte png_get_bit_depth (png_structp png_ptr, png_infop info_ptr)
}
declare 115 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_byte png_get_color_type (png_structp png_ptr, png_infop info_ptr)
}
declare 116 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_byte png_get_filter_type (png_structp png_ptr, png_infop info_ptr)
}
declare 117 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_byte png_get_interlace_type (png_structp png_ptr, png_infop info_ptr)
}
declare 118 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_byte png_get_compression_type (png_structp png_ptr, png_infop info_ptr)
}
declare 119 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_uint_32 png_get_pixels_per_meter (png_structp png_ptr, png_infop info_ptr)
}
declare 120 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_uint_32 png_get_x_pixels_per_meter (png_structp png_ptr, png_infop info_ptr)
}
declare 121 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_uint_32 png_get_y_pixels_per_meter (png_structp png_ptr, png_infop info_ptr)
}
declare 122 generic {!PNG_EASY_ACCESS_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    float png_get_pixel_aspect_ratio (png_structp png_ptr, png_infop info_ptr)
}
declare 123 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_int_32 png_get_x_offset_pixels (png_structp png_ptr, png_infop info_ptr)
}
declare 124 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_int_32 png_get_y_offset_pixels (png_structp png_ptr, png_infop info_ptr)
}
declare 125 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_int_32 png_get_x_offset_microns (png_structp png_ptr, png_infop info_ptr)
}
declare 126 generic {!PNG_EASY_ACCESS_SUPPORTED} {
    png_int_32 png_get_y_offset_microns (png_structp png_ptr, png_infop info_ptr)
}
declare 127 generic {
    png_bytep png_get_signature (png_structp png_ptr, png_infop info_ptr)
}
declare 128 generic {!PNG_READ_bKGD_SUPPORTED} {
    png_uint_32 png_get_bKGD (png_structp png_ptr, png_infop info_ptr, png_color_16p* background)
}
declare 129 generic {!PNG_bKGD_SUPPORTED} {
    void png_set_bKGD (png_structp png_ptr, png_infop info_ptr, png_color_16p background)
}
declare 130 generic {!PNG_READ_cHRM_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    png_uint_32 png_get_cHRM (png_structp png_ptr, png_infop info_ptr,
    	double *white_x, double *white_y, double *red_x, double *red_y,
    	double *green_x, double *green_y, double *blue_x, double *blue_y)
}
declare 131 generic {!PNG_READ_cHRM_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    png_uint_32 png_get_cHRM_fixed (png_structp png_ptr, png_infop info_ptr,
    	png_fixed_point *int_white_x, png_fixed_point *int_white_y, png_fixed_point *int_red_x,
    	png_fixed_point *int_red_y, png_fixed_point *int_green_x, png_fixed_point *int_green_y,
    	png_fixed_point *int_blue_x, png_fixed_point *int_blue_y)
}
declare 132 generic {!PNG_cHRM_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_cHRM (png_structp png_ptr, png_infop info_ptr, double white_x,
    	double white_y, double red_x, double red_y, double green_x, double green_y,
    	double blue_x, double blue_y)
}
declare 133 generic {!PNG_cHRM_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    void png_set_cHRM_fixed (png_structp png_ptr, png_infop info_ptr, png_fixed_point int_white_x,
    	png_fixed_point int_white_y, png_fixed_point int_red_x, png_fixed_point int_red_y,
    	png_fixed_point int_green_x, png_fixed_point int_green_y, png_fixed_point int_blue_x,
    	png_fixed_point int_blue_y)
}
declare 134 generic {!PNG_READ_gAMA_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    png_uint_32 png_get_gAMA (png_structp png_ptr, png_infop info_ptr, double *file_gamma)
}
declare 135 generic {!PNG_READ_gAMA_SUPPORTED} {
    png_uint_32 png_get_gAMA_fixed (png_structp png_ptr, png_infop info_ptr, png_fixed_point* int_file_gamma)
}
declare 136 generic {!PNG_gAMA_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_gAMA (png_structp png_ptr, png_infop info_ptr, double file_gamma)
}
declare 137 generic {!PNG_gAMA_SUPPORTED} {
    void png_set_gAMA_fixed (png_structp png_ptr, png_infop info_ptr, png_fixed_point int_file_gamma)
}
declare 138 generic {!PNG_READ_hIST_SUPPORTED} {
    png_uint_32 png_get_hIST (png_structp png_ptr, png_infop info_ptr, png_uint_16p *hist)
}
declare 139 generic {!PNG_hIST_SUPPORTED} {
    void png_set_hIST (png_structp png_ptr, png_infop info_ptr, png_uint_16p hist)
}
declare 140 generic {
    png_uint_32 png_get_IHDR (png_structp png_ptr, png_infop info_ptr, png_uint_32 *width, png_uint_32 *height, int *bit_depth, int *color_type, int *interlace_type, int *compression_type, int *filter_type)
}
declare 141 generic {
    void png_set_IHDR (png_structp png_ptr, png_infop info_ptr, png_uint_32 width, png_uint_32 height, int bit_depth, int color_type, int interlace_type, int compression_type, int filter_type)
}
declare 142 generic {!PNG_READ_oFFs_SUPPORTED} {
    png_uint_32 png_get_oFFs (png_structp png_ptr, png_infop info_ptr, png_int_32 *offset_x, png_int_32 *offset_y, int *unit_type)
}
declare 143 generic {!PNG_oFFs_SUPPORTED} {
    void png_set_oFFs (png_structp png_ptr, png_infop info_ptr, png_int_32 offset_x, png_int_32 offset_y, int unit_type)
}
declare 144 generic {!PNG_READ_pCAL_SUPPORTED} {
    png_uint_32 png_get_pCAL (png_structp png_ptr, png_infop info_ptr, png_charp *purpose, png_int_32 *X0, png_int_32 *X1, int *type, int *nparams, png_charp *units, png_charpp *params)
}
declare 145 generic {!PNG_pCAL_SUPPORTED} {
    void png_set_pCAL (png_structp png_ptr, png_infop info_ptr, png_charp purpose, png_int_32 X0, png_int_32 X1, int type, int nparams, png_charp units, png_charpp params)
}
declare 146 generic {!PNG_READ_pHYs_SUPPORTED} {
    png_uint_32 png_get_pHYs (png_structp png_ptr, png_infop info_ptr, png_uint_32 *res_x, png_uint_32 *res_y, int *unit_type)
}
declare 147 generic {!PNG_pHYs_SUPPORTED} {
    void png_set_pHYs (png_structp png_ptr, png_infop info_ptr, png_uint_32 res_x, png_uint_32 res_y, int unit_type)
}
declare 148 generic {
    png_uint_32 png_get_PLTE (png_structp png_ptr, png_infop info_ptr, png_colorp *palette, int *num_palette)
}
declare 149 generic {
    void png_set_PLTE (png_structp png_ptr, png_infop info_ptr, png_colorp palette, int num_palette)
}

declare 150 generic {!PNG_READ_sBIT_SUPPORTED} {
    png_uint_32 png_get_sBIT (png_structp png_ptr, png_infop info_ptr, png_color_8p* sig_bit)
}
declare 151 generic {!PNG_sBIT_SUPPORTED} {
    void png_set_sBIT (png_structp png_ptr, png_infop info_ptr, png_color_8p sig_bit)
}
declare 152 generic {!PNG_READ_sRGB_SUPPORTED} {
    png_uint_32 png_get_sRGB (png_structp png_ptr, png_infop info_ptr, int *intent)
}
declare 153 generic {!PNG_sRGB_SUPPORTED} {
    void png_set_sRGB (png_structp png_ptr, png_infop info_ptr, int intent)
}
declare 154 generic {!PNG_sRGB_SUPPORTED} {
    void png_set_sRGB_gAMA_and_cHRM (png_structp png_ptr, png_infop info_ptr, int intent)
}
declare 155 generic {!PNG_READ_iCCP_SUPPORTED} {
    png_uint_32 png_get_iCCP (png_structp png_ptr, png_infop info_ptr, png_charpp name, int *compression_type, png_charpp profile, png_uint_32 *proflen)
}
declare 156 generic {!PNG_iCCP_SUPPORTED} {
    void png_set_iCCP (png_structp png_ptr, png_infop info_ptr, png_charp name, int compression_type, png_charp profile, png_uint_32 proflen)
}
declare 157 generic {!PNG_READ_sPLT_SUPPORTED} {
    png_uint_32 png_get_sPLT (png_structp png_ptr, png_infop info_ptr, png_sPLT_tpp entries)
}
declare 158 generic {!PNG_sPLT_SUPPORTED} {
    void png_set_sPLT (png_structp png_ptr, png_infop info_ptr, png_sPLT_tp entries, int nentries)
}
declare 159 generic {!PNG_READ_TEXT_SUPPORTED} {
    png_uint_32 png_get_text (png_structp png_ptr, png_infop info_ptr, png_textp *text_ptr, int *num_text)
}
declare 160 generic {!PNG_TEXT_SUPPORTED} {
    void png_set_text (png_structp png_ptr, png_infop info_ptr, png_textp text_ptr, int num_text)
}
declare 161 generic {!PNG_READ_tIME_SUPPORTED} {
    png_uint_32 png_get_tIME (png_structp png_ptr, png_infop info_ptr, png_timep *mod_time)
}
declare 162 generic {!PNG_tIME_SUPPORTED} {
    void png_set_tIME (png_structp png_ptr, png_infop info_ptr, png_timep mod_time)
}
declare 163 generic {!PNG_READ_tRNS_SUPPORTED} {
    png_uint_32 png_get_tRNS (png_structp png_ptr, png_infop info_ptr, png_bytep *trans, int *num_trans, png_color_16p *trans_values)
}
declare 164 generic {!PNG_tRNS_SUPPORTED} {
    void png_set_tRNS (png_structp png_ptr, png_infop info_ptr, png_bytep trans, int num_trans, png_color_16p trans_values)
}
declare 165 generic {!PNG_READ_sCAL_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    png_uint_32 png_get_sCAL (png_structp png_ptr, png_infop info_ptr, int *unit, double *width, double *height)
}
declare 166 generic {!PNG_READ_sCAL_SUPPORTED PNG_FLOATING_POINT_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    png_uint_32 png_get_sCAL_s (png_structp png_ptr, png_infop info_ptr, int *unit, png_charpp swidth, png_charpp sheight)
}
declare 167 generic {!PNG_sCAL_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_set_sCAL (png_structp png_ptr, png_infop info_ptr, int unit, double width, double height)
}
declare 168 generic {!PNG_sCAL_SUPPORTED PNG_FLOATING_POINT_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    void png_set_sCAL_s (png_structp png_ptr, png_infop info_ptr, int unit, png_charp swidth, png_charp sheight)
}
declare 169 generic {!PNG_UNKNOWN_CHUNKS_SUPPORTED} {
    void png_set_keep_unknown_chunks (png_structp png_ptr, int keep, png_bytep chunk_list, int num_chunks)
}
declare 170 generic {!PNG_UNKNOWN_CHUNKS_SUPPORTED} {
    void png_set_unknown_chunks (png_structp png_ptr, png_infop info_ptr, png_unknown_chunkp unknowns, int num_unknowns)
}
declare 171 generic {!PNG_UNKNOWN_CHUNKS_SUPPORTED} {
    void png_set_unknown_chunk_location (png_structp png_ptr, png_infop info_ptr, int chunk, int location)
}
declare 172 generic {!PNG_UNKNOWN_CHUNKS_SUPPORTED} {
    png_uint_32 png_get_unknown_chunks (png_structp png_ptr, png_infop info_ptr, png_unknown_chunkpp entries)
}
declare 173 generic {
    void png_set_invalid (png_structp png_ptr, png_infop info_ptr, int mask)
}
declare 174 generic {!PNG_INFO_IMAGE_SUPPORTED} {
    void png_read_png (png_structp png_ptr, png_infop info_ptr, int transforms, voidp params)
}
declare 175 generic {!PNG_INFO_IMAGE_SUPPORTED} {
    void png_write_png (png_structp png_ptr, png_infop info_ptr, int transforms, voidp params)
}
declare 176 generic {!PNG_INTERNAL PNG_USE_GLOBAL_ARRAYS {PNG_NO_EXTERN !PNG_ALWAYS_EXTERN}} {
    png_bytep png_sig_bytes (void)
}
declare 177 generic {
    png_charp png_get_copyright (png_structp png_ptr)
}
declare 178 generic {
    png_charp png_get_header_ver (png_structp png_ptr)
}
declare 179 generic {
    png_charp png_get_header_version (png_structp png_ptr)
}
declare 180 generic {
    png_charp png_get_libpng_ver (png_structp png_ptr)
}
declare 181 generic {!PNG_INTERNAL PNG_READ_BIG_ENDIAN_SUPPORTED {!PNG_READ_pCAL_SUPPORTED !PNG_READ_oFFs_SUPPORTED}} {
    png_int_32 png_get_int_32 (png_bytep buf)
}
declare 182 generic {!PNG_INTERNAL PNG_READ_BIG_ENDIAN_SUPPORTED} {
    png_uint_32 png_get_uint_32 (png_bytep buf)
}
declare 183 generic {!PNG_INTERNAL PNG_READ_BIG_ENDIAN_SUPPORTED} {
    png_uint_16 png_get_uint_16 (png_bytep buf)
}
declare 184 generic {!PNG_INTERNAL} {
    void png_read_init (png_structp png_ptr)
}
declare 185 generic {!PNG_INTERNAL} {
    void png_read_init_2 (png_structp png_ptr, png_const_charp user_png_ver, png_size_t png_struct_size, png_size_t png_info_size)
}
declare 186 generic {!PNG_INTERNAL} {
    void png_write_init (png_structp png_ptr)
}
declare 187 generic {!PNG_INTERNAL} {
    void png_write_init_2 (png_structp png_ptr, png_const_charp user_png_ver, png_size_t png_struct_size, png_size_t png_info_size)
}
declare 188 generic {!PNG_INTERNAL} {
    png_voidp png_create_struct (int type)
}
declare 189 generic {!PNG_INTERNAL} {
    void png_destroy_struct (png_voidp struct_ptr)
}
declare 190 generic {!PNG_INTERNAL} {
    png_voidp png_create_struct_2 (int type, png_malloc_ptr malloc_fn)
}
declare 191 generic {!PNG_INTERNAL} {
    void png_destroy_struct_2 (png_voidp struct_ptr, png_free_ptr free_fn)
}
declare 192 generic {!PNG_INTERNAL} {
    void png_info_destroy (png_structp png_ptr, png_infop info_ptr)
}
declare 193 generic {!PNG_INTERNAL} {
    voidpf png_zalloc (voidpf png_ptr, uInt items, uInt size)
}
declare 194 generic {!PNG_INTERNAL} {
    void png_zfree (voidpf png_ptr, voidpf ptr)
}
declare 195 generic {!PNG_INTERNAL} {
    void png_reset_crc (png_structp png_ptr)
}
declare 196 generic {!PNG_INTERNAL} {
    void png_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
}
declare 197 generic {!PNG_INTERNAL} {
    void png_read_data (png_structp png_ptr, png_bytep data, png_size_t length)
}
declare 198 generic {!PNG_INTERNAL} {
    void png_crc_read (png_structp png_ptr, png_bytep buf, png_size_t length)
}
declare 199 generic {!PNG_INTERNAL {!PNG_READ_zTXt_SUPPORTED !PNG_READ_iTXt_SUPPORTED !PNG_READ_iCCP_SUPPORTED !PNG_READ_sPLT_SUPPORTED}} {
    png_charp png_decompress_chunk (png_structp png_ptr, int comp_type, png_charp chunkdata, png_size_t chunklength, png_size_t prefix_length, png_size_t *data_length)
}
declare 200 generic {!PNG_INTERNAL} {
    int png_crc_finish (png_structp png_ptr, png_uint_32 skip)
}
declare 201 generic {!PNG_INTERNAL} {
    int png_crc_error (png_structp png_ptr)
}
declare 202 generic {!PNG_INTERNAL} {
    void png_calculate_crc (png_structp png_ptr, png_bytep ptr, png_size_t length)
}
declare 203 generic {!PNG_INTERNAL !PNG_WRITE_FLUSH_SUPPORTED} {
    void png_flush (png_structp png_ptr)
}
declare 204 generic {!PNG_INTERNAL} {
    void png_save_uint_32 (png_bytep buf, png_uint_32 i)
}
declare 205 generic {!PNG_INTERNAL !PNG_WRITE_pCAL_SUPPORTED} {
    void png_save_int_32 (png_bytep buf, png_int_32 i)
}
declare 206 generic {!PNG_INTERNAL} {
    void png_save_uint_16 (png_bytep buf, unsigned int i)
}
declare 207 generic {!PNG_INTERNAL} {
    void png_write_sig (png_structp png_ptr)
}
declare 208 generic {!PNG_INTERNAL} {
    void png_write_IHDR (png_structp png_ptr, png_uint_32 width, png_uint_32 height, int bit_depth, int color_type, int compression_type, int filter_type, int interlace_type)
}
declare 209 generic {!PNG_INTERNAL} {
    void png_write_PLTE (png_structp png_ptr, png_colorp palette, png_uint_32 num_pal)
}
declare 210 generic {!PNG_INTERNAL} {
    void png_write_IDAT (png_structp png_ptr, png_bytep data, png_size_t length)
}
declare 211 generic {!PNG_INTERNAL} {
    void png_write_IEND (png_structp png_ptr)
}
declare 212 generic {!PNG_INTERNAL !PNG_WRITE_gAMA_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_write_gAMA (png_structp png_ptr, double file_gamma)
}
declare 213 generic {!PNG_INTERNAL !PNG_WRITE_gAMA_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    void png_write_gAMA_fixed (png_structp png_ptr, png_fixed_point file_gamma)
}
declare 214 generic {!PNG_INTERNAL !PNG_WRITE_sBIT_SUPPORTED} {
    void png_write_sBIT (png_structp png_ptr, png_color_8p sbit, int color_type)
}
declare 215 generic {!PNG_INTERNAL !PNG_WRITE_cHRM_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED} {
    void png_write_cHRM (png_structp png_ptr, double white_x, double white_y, double red_x, double red_y, double green_x, double green_y, double blue_x, double blue_y)
}
declare 216 generic {!PNG_INTERNAL !PNG_WRITE_cHRM_SUPPORTED !PNG_FIXED_POINT_SUPPORTED} {
    void png_write_cHRM_fixed (png_structp png_ptr, png_fixed_point int_white_x, png_fixed_point int_white_y, png_fixed_point int_red_x, png_fixed_point int_red_y, png_fixed_point int_green_x, png_fixed_point int_green_y, png_fixed_point int_blue_x, png_fixed_point int_blue_y)
}
declare 217 generic {!PNG_INTERNAL !PNG_WRITE_sRGB_SUPPORTED} {
    void png_write_sRGB (png_structp png_ptr, int intent)
}
declare 218 generic {!PNG_INTERNAL !PNG_WRITE_iCCP_SUPPORTED} {
    void png_write_iCCP (png_structp png_ptr, png_charp name, int compression_type, png_charp profile, int proflen)
}
declare 219 generic {!PNG_INTERNAL !PNG_WRITE_sPLT_SUPPORTED} {
    void png_write_sPLT (png_structp png_ptr, png_sPLT_tp palette)
}
declare 220 generic {!PNG_INTERNAL !PNG_WRITE_tRNS_SUPPORTED} {
    void png_write_tRNS (png_structp png_ptr, png_bytep trans, png_color_16p values, int number, int color_type)
}
declare 221 generic {!PNG_INTERNAL !PNG_WRITE_bKGD_SUPPORTED} {
    void png_write_bKGD (png_structp png_ptr, png_color_16p values, int color_type)
}
declare 222 generic {!PNG_INTERNAL !PNG_WRITE_hIST_SUPPORTED} {
    void png_write_hIST (png_structp png_ptr, png_uint_16p hist, int num_hist)
}
declare 223 generic {!PNG_INTERNAL {!PNG_WRITE_TEXT_SUPPORTED !PNG_WRITE_pCAL_SUPPORTED !PNG_WRITE_iCCP_SUPPORTED !PNG_WRITE_sPLT_SUPPORTED}} {
    png_size_t png_check_keyword (png_structp png_ptr, png_charp key, png_charpp new_key)
}
declare 224 generic {!PNG_INTERNAL !PNG_WRITE_tEXt_SUPPORTED} {
    void png_write_tEXt (png_structp png_ptr, png_charp key, png_charp text, png_size_t text_len)
}
declare 225 generic {!PNG_INTERNAL !PNG_WRITE_zTXt_SUPPORTED} {
    void png_write_zTXt (png_structp png_ptr, png_charp key, png_size_t text_len, int compression)
}
declare 226 generic {!PNG_INTERNAL !PNG_iTXt_SUPPORTED} {
    void png_write_iTXt (png_structp png_ptr, int compression, png_charp key, png_charp lang, png_charp lang_key, png_charp text)
}
declare 227 generic {!PNG_INTERNAL !PNG_WRITE_oFFs_SUPPORTED} {
    void png_write_oFFs (png_structp png_ptr, png_uint_32 x_offset, png_uint_32 y_offset, int unit_type)
}
declare 228 generic {!PNG_INTERNAL !PNG_WRITE_pCAL_SUPPORTED} {
    void png_write_pCAL (png_structp png_ptr, png_charp purpose, png_int_32 X0, png_int_32 X1, int type, int nparams, png_charp units, png_charpp params)
}
declare 229 generic {!PNG_INTERNAL !PNG_WRITE_pHYs_SUPPORTED} {
    void png_write_pHYs (png_structp png_ptr, png_uint_32 x_pixels_per_unit, png_uint_32 y_pixels_per_unit, int unit_type)
}
declare 230 generic {!PNG_INTERNAL !PNG_WRITE_tIME_SUPPORTED} {
    void png_write_tIME (png_structp png_ptr, png_timep mod_time)
}
declare 231 generic {!PNG_INTERNAL !PNG_WRITE_sCAL_SUPPORTED !PNG_FLOATING_POINT_SUPPORTED PNG_NO_STDIO} {
    void png_write_sCAL (png_structp png_ptr, int unit, double width, double height)
}
declare 232 generic {!PNG_INTERNAL !PNG_FIXED_POINT_SUPPORTED {PNG_WRITE_sCAL_SUPPORTED PNG_FLOATING_POINT_SUPPORTED !PNG_NO_STDIO}} {
    void png_write_sCAL_s (png_structp png_ptr, int unit, png_charp width, png_charp height)
}
declare 233 generic {!PNG_INTERNAL} {
    void png_write_finish_row (png_structp png_ptr)
}
declare 234 generic {!PNG_INTERNAL} {
    void png_write_start_row (png_structp png_ptr)
}
declare 235 generic {!PNG_INTERNAL !PNG_READ_GAMMA_SUPPORTED} {
    void png_build_gamma_table (png_structp png_ptr)
}
declare 236 generic {!PNG_INTERNAL} {
    void png_combine_row (png_structp png_ptr, png_bytep row, int mask)
}
declare 237 generic {!PNG_INTERNAL !PNG_READ_INTERLACING_SUPPORTED} {
    void png_do_read_interlace (png_row_infop row_info, png_bytep row, int pass, png_uint_32 transformations)
}
declare 238 generic {!PNG_INTERNAL !PNG_WRITE_INTERLACING_SUPPORTED} {
    void png_do_write_interlace (png_row_infop row_info, png_bytep row, png_bytep prev_row, int filter)
}
declare 239 generic {!PNG_INTERNAL} {
    void png_read_filter_row (png_structp png_ptr, png_row_infop row_info, png_bytep row, png_bytep prev_row, int filter)
}
declare 240 generic {!PNG_INTERNAL} {
    void png_write_find_filter (png_structp png_ptr, png_row_infop row_info)
}
declare 241 generic {!PNG_INTERNAL} {
    void png_write_filtered_row (png_structp png_ptr, png_bytep filtered_row)
}
declare 242 generic {!PNG_INTERNAL} {
    void png_read_finish_row (png_structp png_ptr)
}
declare 243 generic {!PNG_INTERNAL} {
    void png_read_start_row (png_structp png_ptr)
}
declare 244 generic {!PNG_INTERNAL} {
    void png_read_transform_info (png_structp png_ptr, png_infop info_ptr)
}
declare 245 generic {!PNG_INTERNAL !PNG_READ_FILLER_SUPPORTED} {
    void png_do_read_filler (png_row_infop row_info, png_bytep row, png_uint_32 filler, png_uint_32 flags)
}
declare 246 generic {!PNG_INTERNAL !PNG_READ_SWAP_ALPHA_SUPPORTED} {
    void png_do_read_swap_alpha (png_row_infop row_info, png_bytep row)
}
declare 247 generic {!PNG_INTERNAL !PNG_WRITE_SWAP_ALPHA_SUPPORTED} {
    void png_do_write_swap_alpha (png_row_infop row_info, png_bytep row)
}
declare 248 generic {!PNG_INTERNAL !PNG_READ_INVERT_ALPHA_SUPPORTED} {
    void png_do_read_invert_alpha (png_row_infop row_info, png_bytep row)
}
declare 249 generic {!PNG_INTERNAL !PNG_WRITE_INVERT_ALPHA_SUPPORTED} {
    void png_do_write_invert_alpha (png_row_infop row_info, png_bytep row)
}
declare 250 generic {!PNG_INTERNAL {!PNG_WRITE_FILLER_SUPPORTED !PNG_READ_STRIP_ALPHA_SUPPORTED}} {
    void png_do_strip_filler (png_row_infop row_info, png_bytep row, png_uint_32 flags)
}
declare 251 generic {!PNG_INTERNAL {!PNG_READ_SWAP_SUPPORTED !PNG_WRITE_SWAP_SUPPORTED}} {
    void png_do_swap (png_row_infop row_info, png_bytep row)
}
declare 252 generic {!PNG_INTERNAL {!PNG_READ_PACKSWAP_SUPPORTED !PNG_WRITE_PACKSWAP_SUPPORTED}} {
    void png_do_packswap (png_row_infop row_info, png_bytep row)
}
declare 253 generic {!PNG_INTERNAL !PNG_READ_RGB_TO_GRAY_SUPPORTED} {
    int png_do_rgb_to_gray (png_structp png_ptr, png_row_infop row_info, png_bytep row)
}
declare 254 generic {!PNG_INTERNAL !PNG_READ_GRAY_TO_RGB_SUPPORTED} {
    void png_do_gray_to_rgb (png_row_infop row_info, png_bytep row)
}
declare 255 generic {!PNG_INTERNAL !PNG_READ_PACK_SUPPORTED} {
    void png_do_unpack (png_row_infop row_info, png_bytep row)
}
declare 256 generic {!PNG_INTERNAL !PNG_READ_SHIFT_SUPPORTED} {
    void png_do_unshift (png_row_infop row_info, png_bytep row, png_color_8p sig_bits)
}
declare 257 generic {!PNG_INTERNAL {!PNG_READ_INVERT_SUPPORTED !PNG_WRITE_INVERT_SUPPORTED}} {
    void png_do_invert (png_row_infop row_info, png_bytep row)
}
declare 258 generic {!PNG_INTERNAL !PNG_READ_16_TO_8_SUPPORTED} {
    void png_do_chop (png_row_infop row_info, png_bytep row)
}
declare 259 generic {!PNG_INTERNAL !PNG_READ_DITHER_SUPPORTED} {
    void png_do_dither (png_row_infop row_info, png_bytep row, png_bytep palette_lookup, png_bytep dither_lookup)
}
declare 260 generic {!PNG_INTERNAL !PNG_READ_DITHER_SUPPORTED !PNG_CORRECT_PALETTE_SUPPORTED} {
    void png_correct_palette (png_structp png_ptr, png_colorp palette, int num_palette)
}
declare 261 generic {!PNG_INTERNAL {!PNG_READ_BGR_SUPPORTED !PNG_WRITE_BGR_SUPPORTED}} {
    void png_do_bgr (png_row_infop row_info, png_bytep row)
}
declare 262 generic {!PNG_INTERNAL !PNG_WRITE_PACK_SUPPORTED} {
    void png_do_pack (png_row_infop row_info, png_bytep row, png_uint_32 bit_depth)
}
declare 263 generic {!PNG_INTERNAL !PNG_WRITE_SHIFT_SUPPORTED} {
    void png_do_shift (png_row_infop row_info, png_bytep row, png_color_8p bit_depth)
}
declare 264 generic {!PNG_INTERNAL !PNG_READ_BACKGROUND_SUPPORTED} {
    void png_do_background (png_row_infop row_info, png_bytep row, png_color_16p trans_values, png_color_16p background, png_color_16p background_1, png_bytep gamma_table, png_bytep gamma_from_1, png_bytep gamma_to_1, png_uint_16pp gamma_16, png_uint_16pp gamma_16_from_1, png_uint_16pp gamma_16_to_1, int gamma_shift)
}
declare 265 generic {!PNG_INTERNAL !PNG_READ_GAMMA_SUPPORTED} {
    void png_do_gamma (png_row_infop row_info, png_bytep row, png_bytep gamma_table, png_uint_16pp gamma_16_table, int gamma_shift)
}
declare 266 generic {!PNG_INTERNAL !PNG_READ_EXPAND_SUPPORTED} {
    void png_do_expand_palette (png_row_infop row_info, png_bytep row, png_colorp palette, png_bytep trans, int num_trans)
}
declare 267 generic {!PNG_INTERNAL !PNG_READ_EXPAND_SUPPORTED} {
    void png_do_expand (png_row_infop row_info, png_bytep row, png_color_16p trans_value)
}
declare 268 generic {!PNG_INTERNAL} {
    void png_handle_IHDR (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 269 generic {!PNG_INTERNAL} {
    void png_handle_PLTE (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 270 generic {!PNG_INTERNAL} {
    void png_handle_IEND (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 271 generic {!PNG_INTERNAL !PNG_READ_bKGD_SUPPORTED} {
    void png_handle_bKGD (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 272 generic {!PNG_INTERNAL !PNG_READ_cHRM_SUPPORTED} {
    void png_handle_cHRM (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 273 generic {!PNG_INTERNAL !PNG_READ_gAMA_SUPPORTED} {
    void png_handle_gAMA (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 274 generic {!PNG_INTERNAL !PNG_READ_hIST_SUPPORTED} {
    void png_handle_hIST (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 275 generic {!PNG_INTERNAL !PNG_READ_iCCP_SUPPORTED} {
    void png_handle_iCCP (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 276 generic {!PNG_INTERNAL !PNG_iTXt_SUPPORTED} {
    void png_handle_iTXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 277 generic {!PNG_INTERNAL !PNG_READ_oFFs_SUPPORTED} {
    void png_handle_oFFs (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 278 generic {!PNG_INTERNAL !PNG_READ_pCAL_SUPPORTED} {
    void png_handle_pCAL (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 279 generic {!PNG_INTERNAL !PNG_READ_pHYs_SUPPORTED} {
    void png_handle_pHYs (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 280 generic {!PNG_INTERNAL !PNG_READ_sBIT_SUPPORTED} {
    void png_handle_sBIT (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 281 generic {!PNG_INTERNAL !PNG_READ_sCAL_SUPPORTED} {
    void png_handle_sCAL (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 282 generic {!PNG_INTERNAL !PNG_READ_sPLT_SUPPORTED} {
    void png_handle_sPLT (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 283 generic {!PNG_INTERNAL !PNG_READ_sRGB_SUPPORTED} {
    void png_handle_sRGB (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 284 generic {!PNG_INTERNAL !PNG_READ_tEXt_SUPPORTED} {
    void png_handle_tEXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 285 generic {!PNG_INTERNAL !PNG_READ_tIME_SUPPORTED} {
    void png_handle_tIME (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 286 generic {!PNG_INTERNAL !PNG_READ_tRNS_SUPPORTED} {
    void png_handle_tRNS (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 287 generic {!PNG_INTERNAL !PNG_READ_zTXt_SUPPORTED} {
    void png_handle_zTXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 288 generic {!PNG_INTERNAL !PNG_HANDLE_AS_UNKNOWN_SUPPORTED} {
    int png_handle_as_unknown (png_structp png_ptr, png_bytep chunk_name)
}
declare 289 generic {!PNG_INTERNAL} {
    void png_handle_unknown (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 290 generic {!PNG_INTERNAL} {
    void png_check_chunk_name (png_structp png_ptr, png_bytep chunk_name)
}
declare 291 generic {!PNG_INTERNAL} {
    void png_do_read_transformations (png_structp png_ptr)
}
declare 292 generic {!PNG_INTERNAL} {
    void png_do_write_transformations (png_structp png_ptr)
}
declare 293 generic {!PNG_INTERNAL} {
    void png_init_read_transformations (png_structp png_ptr)
}
declare 294 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_read_chunk (png_structp png_ptr, png_infop info_ptr)
}
declare 295 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_read_sig (png_structp png_ptr, png_infop info_ptr)
}
declare 296 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_check_crc (png_structp png_ptr)
}
declare 297 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_crc_skip (png_structp png_ptr, png_uint_32 length)
}
declare 298 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_crc_finish (png_structp png_ptr)
}
declare 299 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_fill_buffer (png_structp png_ptr, png_bytep buffer, png_size_t length)
}
declare 300 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_save_buffer (png_structp png_ptr)
}
declare 301 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_restore_buffer (png_structp png_ptr, png_bytep buffer, png_size_t buffer_length)
}
declare 302 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_read_IDAT (png_structp png_ptr)
}
declare 303 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_process_IDAT_data (png_structp png_ptr, png_bytep buffer, png_size_t buffer_length)
}
declare 304 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_process_row (png_structp png_ptr)
}
declare 305 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_handle_unknown (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 306 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_have_info (png_structp png_ptr, png_infop info_ptr)
}
declare 307 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_have_end (png_structp png_ptr, png_infop info_ptr)
}
declare 308 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_have_row (png_structp png_ptr, png_bytep row)
}
declare 309 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_push_read_end (png_structp png_ptr, png_infop info_ptr)
}
declare 310 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_process_some_data (png_structp png_ptr, png_infop info_ptr)
}
declare 311 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED} {
    void png_read_push_finish_row (png_structp png_ptr)
}
declare 312 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_tEXt_SUPPORTED} {
    void png_push_handle_tEXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 313 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_tEXt_SUPPORTED} {
    void png_push_read_tEXt (png_structp png_ptr, png_infop info_ptr)
}
declare 314 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_zTXt_SUPPORTED} {
    void png_push_handle_zTXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 315 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_zTXt_SUPPORTED} {
    void png_push_read_zTXt (png_structp png_ptr, png_infop info_ptr)
}
declare 316 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_iTXt_SUPPORTED} {
    void png_push_handle_iTXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}
declare 317 generic {!PNG_INTERNAL !PNG_PROGRESSIVE_READ_SUPPORTED !PNG_iTXt_SUPPORTED} {
    void png_push_read_iTXt (png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
}

#########################################################################
