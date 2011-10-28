/*                    O B J _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file obj_parser.h
 *
 * Interface to Wavefront OBJ Parser.
 *
 */
#ifndef ARL_OBJ_PARSER_H
#define ARL_OBJ_PARSER_H

#include "common.h"
#include <sys/types.h>
#include <stdio.h>

__BEGIN_DECLS

/**
 *  A structure containing a wavefront obj parser.
 *  This is a C wrapper for objParser.
 */
typedef struct {
    void *p;
} obj_parser_t;

/**
 *  A structure containing the contents of a parsed wavefront obj file.
 *  This is a C wrapper for objFileContents.
 */
typedef struct {
    void *p;
} obj_contents_t;

/**
 *  A structure with the attributes describing a polygonal element
 *
 *  groupset_index
 *  An index into a set of group sets this element belongs to.
 *  See obj_groups and obj_groupsets
 *
 *  object_index
 *  An index into a set of object labels that this element belongs to
 *  See obj_objects
 *
 *  material_index
 *  An index into a set of materials that describes this element
 *  See obj_materials
 *
 *  materiallibset_index
 *  An index into a set of material library sets this element uses
 *  See obj_materiallibs and obj_materiallibsets
 *
 *  texmap_index
 *  An index into a set of texture maps that describes this element
 *  See obj_texmaps
 *
 *  texmaplibset_index
 *  An index into a set of texture map library sets this element uses
 *  See obj_texmaplibs and obj_texmaplibsets
 *
 *  shadow_obj_index
 *  An index into a set of shadow objects that this element uses
 *  See obj_shadow_objs
 *
 *  trace_obj_index
 *  An index into a set of trace objects that this element uses
 *  See obj_trace_objs
 *
 *  smooth_group
 *  A smoothing group id for this element
 *  N.B. unspecified in signess or default in wavefront obj spec,
 *  here unsigned and 0
 *
 *  bevel
 *  Whether this element bevel flag is off (zero) or on (non-zero), as per
 *  wavefront spec, default is off
 *
 *  c_interp
 *  Whether color interpolation is off (zero) or on (non-zero), as per
 *  wavefront spec, default is off
 *
 *  d_interp
 *  Whether dissolve interpolation is off (zero) or on (non-zero), as per
 *  wavefront spec, default is off
 *
 *  lod
 *  The level of detail this element should be displayed as. This is a value
 *  between 0 (all elements) and 100. As per wavefront spec, default is 0.
 */
typedef struct {
    size_t groupset_index;
    size_t object_index;
    size_t material_index;
    size_t materiallibset_index;
    size_t texmap_index;
    size_t texmaplibset_index;
    size_t shadow_obj_index;
    size_t trace_obj_index;
    unsigned int smooth_group;
    unsigned char bevel;
    unsigned char c_interp;
    unsigned char d_interp;
    unsigned char lod;
} obj_polygonal_attributes_t;

/**
 *  Allocate a obj_parser_t object.
 *
 *  You must eventually call obj_parser_destroy.
 *
 *  Return values:
 *  0 - success
 *  ENOMEM - out of memory
 */
int obj_parser_create(obj_parser_t *parser);

/**
 *  Destroy the obj_parser_t object.
 */
void obj_parser_destroy(obj_parser_t parser);

/**
 *  Parse the obj file 'filename' with 'parser', create and place results into
 *  'contents'.
 *
 *  If the filename begins with the '/' character, then the file is
 *  understood to be an absolute path starting at the root directory. If the
 *  filename does not begin with a '/' character, the file is understood to
 *  be relative to the current working directory. In either case any additional
 *  relative file inclusion mechanism will be relative to the directory that
 *  holds the input file.
 *
 *  After a successful call to obj_parse_file, you must eventually
 *  call obj_contents_destroy.
 *
 *  If unable to successfully parse 'filename' a negative value will be
 *  returned and the reason can be obtained by obj_parse_error.
 *
 *  Warnings or information about non-implemented features will return success
 *  and be reported via obj_parse_error.
 *
 *  Return values:
 *  0 - success
 *  ENOMEM - out of memory
 *  Any error code returned by fopen
 *  <0 - failure, see obj_parse_error
 */
int obj_parse(const char *filename, obj_parser_t parser,
	      obj_contents_t *contents);

/**
 *  Parse the obj file stream pointed to by 'stream' with 'parser', create and
 *  place results into 'contents'.
 *
 *  Operational semantics with respect to 'stream' will mimic 'fread' and
 *  shall not close 'stream' under any circumstance.
 *
 *  From IEEE Std 1003.1-2001:
 *    The file position indicator for the stream (if defined) shall be advanced
 *    by the number of bytes successfully read. If an error occurs, the
 *    resulting value of the file position indicator for the stream is
 *    unspecified. If a partial element is read, its value is unspecified.
 *
 *  Any additional relative file inclusion mechanism will be relative to the
 *  current working directory.
 *
 *  After a successful call to obj_parse_file, you must eventually
 *  call obj_contents_destroy.
 *
 *  If unable to successfully parse the stream pointed to by 'stream' a
 *  negative value will be returned and the reason can be obtained by
 *  obj_parse_error.
 *
 *  Warnings or information about non-implemented features will return success
 *  and be reported via obj_parse_error.
 *
 *  Return values:
 *  0 - success
 *  ENOMEM - out of memory
 *  Any error code returned by fopen
 *  <0 - failure, see obj_parse_error
 */
int obj_fparse(FILE *stream, obj_parser_t parser, obj_contents_t *contents);

/**
 *  Return the reason the last attempt to parse a file failed or warnings
 *  generated during the parse.
 *
 *  Return:
 *  0 - The previous parse attempt succeeded and no warnings generated.
 *  !0 - A null terminated string containing information about the last parse 
 */
const char * obj_parse_error(obj_parser_t parser);

/**
 *  Destroy the obj_contents_t object.
 *
 *  Return values:
 *  0 - success
 */
int obj_contents_destroy(obj_contents_t contents);

/**
 *  Obtain a list of all vertices contained in 'contents'.
 *
 *  Copy a pointer to a multidimensional array of vertices contained in 
 *  'contents' to the location pointed to by val_arr and return its length.
 *
 *  The vertices are contained in an array of type const float[][4] and must
 *  not be modified by the caller. The format of the second dimension is:
 *  {x y z w} where w is 1.0 by default.
 *
 *  Return value:
 *  The length of the vertex list
 */
size_t obj_vertices(obj_contents_t contents, const float (*val_arr[])[4]);

/**
 *  Obtain a list of all texture vertices contained in 'contents'.
 *
 *  Copy a pointer to a multidimensional array of texture vertices contained in 
 *  'contents' to the location pointed to by val_arr and return its length.
 *
 *  The vertices are contained in an array of type const float[][3] and must
 *  not be modified by the caller. The format of the second dimension is:
 *  {u v w} where v and w 0 by default.
 *
 *  Return value:
 *  The length of the texture vertex list
 */
size_t obj_texture_coord(obj_contents_t contents, const float (*val_arr[])[3]);

/**
 *  Obtain a list of all normals contained in 'contents'.
 *
 *  Copy a pointer to a multidimensional array of normals contained in 
 *  'contents' to the location pointed to by val_arr and return its length.
 *
 *  The normals are contained in an array of type const float[][3] and must
 *  not be modified by the caller. The format of the second dimension is:
 *  {i j j}.
 *
 *  Return value:
 *  The length of the normal list
 */
size_t obj_normals(obj_contents_t contents, const float (*val_arr[])[3]);

/**
 *  Obtain a unique set of all group names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of group names
 *  contained in 'contents' to the location pointed to by val_arr and return
 *  its length.
 *
 *  Group names are guaranteed to be a unique set.
 *  
 *  If an element has not been explicitly tagged as being part of a particular
 *  group, per the wavefront obj spec, the element is placed in the "default"
 *  group.
 *
 *  Return value:
 *  The length of the group name list.
 */
size_t obj_groups(obj_contents_t contents, const char * const (*val_arr[]));

/**
 *  Return the number of group sets contained in 'contents'
 *
 *  Return value:
 *  The number of group sets
 */
size_t obj_num_groupsets(obj_contents_t contents);

/**
 *  Obtain the 'n'th group set contained in 'contents'
 *
 *  Copy a pointer to an index set of group indices for the 'n'th set
 *  contained in 'contents' to the location pointed to by val_arr and return
 *  its length.
 *
 *  Group sets are guaranteed to be a unique set.
 *  
 *  When an element references a groupset index of 'n', calling obj_groupset
 *  with the 'n'th set will obtain a unique set of indices that can be used
 *  to obtain the group name.
 *
 *  Return value:
 *  The length of the group set.
 */
size_t obj_groupset(obj_contents_t contents, size_t n,
		    const size_t (*index_arr[]));

/**
 *  Obtain a unique set of all object names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of object
 *  names contained in 'contents' to the location pointed to by val_arr and
 *  return its length.
 *
 *  Object names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as being a part of an object. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  being part of a particular object, the element shall have the "" object
 *  tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  object name to the empty string, the "" tag is unique to elements that
 *  have not had the object explicitly set.
 *
 *  Return value:
 *  The length of the object name list.
 */
size_t obj_objects(obj_contents_t contents, const char * const (*val_arr[]));

/**
 *  Obtain a unique set of all material names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of material
 *  names contained in 'contents' to the location pointed to by val_arr and
 *  return its length.
 *
 *  Material names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as having a material trait. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  having a material trait, the element shall have the "" material trait tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  material name to the empty string, the "" tag is unique to elements that
 *  have not had the material trait explicitly set.
 *
 *  Return value:
 *  The length of the material name list.
 */
size_t obj_materials(obj_contents_t contents, const char * const (*val_arr[]));

/**
 *  Obtain a unique set of all material library names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of material
 *  library names contained in 'contents' to the location pointed to by val_arr
 *  and return its length.
 *
 *  Material library names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as using a material library. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  using a material library, the element shall have the "" material library
 *  tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  material library to the empty string, the "" tag is unique to elements that
 *  have not had the material library explicitly set.
 *
 *  Return value:
 *  The length of the material library name list.
 */
size_t obj_materiallibs(obj_contents_t contents,
			const char * const (*val_arr[]));

/**
 *  Return the number of material library sets contained in 'contents'
 *
 *  Return value:
 *  The number of material library sets
 */
size_t obj_num_materiallibsets(obj_contents_t contents);

/**
 *  Obtain the 'n'th material libary set contained in 'contents'
 *
 *  Copy a pointer to an index set of material library indices for the 'n'th
 *  set contained in 'contents' to the location pointed to by val_arr and
 *  return its length.
 *
 *  Material library sets are guaranteed to be a unique set.
 *  
 *  When an element references a materiallibset index of 'n', calling
 *  obj_materiallibset with the 'n'th set will obtain a unique set of indices
 *  that can be used to obtain the material library name.
 *
 *  Return value:
 *  The length of the material library set.
 */
size_t obj_materiallibset(obj_contents_t contents, size_t n,
			  const size_t (*index_arr[]));

/**
 *  Obtain a unique set of all texture map names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of texture map
 *  names contained in 'contents' to the location pointed to by val_arr and
 *  return its length.
 *
 *  Texture map names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as having a texture map. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  having a texture map, the element shall have the "" texture map tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  texture map name to the empty string, the "" tag is unique to elements that
 *  have not had a texture map explicitly set.
 *
 *  Return value:
 *  The length of the texture map list.
 */
size_t obj_texmaps(obj_contents_t contents, const char * const (*val_arr[]));

/**
 *  Obtain a unique set of all texture map library names contained in
 *  'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of texture map
 *  library names contained in 'contents' to the location pointed to by val_arr
 *  and return its length.
 *
 *  Texture map library names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as using a texture map library. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  using a texture map library, the element shall have the "" material library
 *  tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  texture map library to the empty string, the "" tag is unique to elements
 *  that have not had the texture mao library explicitly set.
 *
 *  Return value:
 *  The length of the texture map library name list.
 */
size_t obj_texmaplibs(obj_contents_t contents,
		      const char * const (*val_arr[]));

/**
 *  Return the number of texture map library sets contained in 'contents'
 *
 *  Return value:
 *  The number of texture map library sets
 */
size_t obj_num_texmaplibsets(obj_contents_t contents);

/**
 *  Obtain the 'n'th texture map libary set contained in 'contents'
 *
 *  Copy a pointer to an index set of texture map library indices for the 'n'th
 *  set contained in 'contents' to the location pointed to by val_arr and
 *  return its length.
 *
 *  Texture map library sets are guaranteed to be a unique set.
 *  
 *  When an element references a texmaplibset index of 'n', calling
 *  obj_textmaplibset with the 'n'th set will obtain a unique set of indices
 *  that can be used to obtain the texture map library name.
 *
 *  Return value:
 *  The length of the texture map library set.
 */
size_t obj_texmaplibset(obj_contents_t contents, size_t set,
			const size_t (*index_arr[]));

/**
 *  Obtain a unique set of all shadow object names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of shadow
 *  object names contained in 'contents' to the location pointed to by val_arr
 *  and return its length.
 *
 *  Shadow object names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as having a shadow object. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  having a shadow object, the element shall have the "" shadow object tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  shadow object name to the empty string, the "" tag is unique to elements
 *  that have not had the shadow object explicitly set.
 *
 *  Return value:
 *  The length of the shadow object name list.
 */
size_t obj_shadow_objs(obj_contents_t contents,
		       const char * const (*val_arr[]));

/**
 *  Obtain a unique set of all trace object names contained in 'contents'
 *
 *  Copy a pointer to a null terminated character sequence array of trace
 *  object names contained in 'contents' to the location pointed to by val_arr
 *  and return its length.
 *
 *  Trace object names are guaranteed to be a unique set.
 *  
 *  The wavefront obj spec does not specify the default behavior for elements
 *  not explicitly being defined as having a trace object. Therefore,
 *  for this implementation, if an element has not been explicitly tagged as
 *  having a trace object, the element shall have the "" trace object tag.
 *
 *  N.B. As it is impossible as per the wavefront obj spec to set a particular
 *  trace object name to the empty string, the "" tag is unique to elements
 *  that have not had the trace object explicitly set.
 *
 *  Return value:
 *  The length of the trace object name list.
 */
size_t obj_trace_objs(obj_contents_t contents,
		      const char * const (*val_arr[]));

/**
 *  Obtain a list of polygonal attributes in 'contents'
 *
 *  Copy a pointer to a const obj_polygonal_attributes_t array of polygonal
 *  attrbutes contained in 'contents' to the location pointed to by attr_arr
 *  and return its length.
 *
 *  Polygonal attributes are guaranteed to be a unique set.
 *
 *  The ith index corresponds to to ith index of attindex obtained via one of
 *  the obj_polygonal_*_faces functions. The member value of the ith element
 *  obtained via this function is the index into the '*_list' array obtained
 *  via corresponding the obj_* function.
 *
 *  Return value
 *  The length of the polygonal attributes list
 */
size_t obj_polygonal_attributes(obj_contents_t contents,
    const obj_polygonal_attributes_t (*attr_list[]));

/**
 *  Obtain the list of polygonal attributes for all polygonal points
 *  in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes describing the
 *  polygonal attributes for the ith point.
 *
 *  Return value
 *  The total number of polygonal points
 */
size_t obj_polygonal_v_points(obj_contents_t contents,
			      const size_t (*attindex_arr[]));

/**
 *  Obtain the vertex indices for the 'n'th polygonal point only identifed by
 *  vertices in 'contents'
 *
 *  Copy a pointer to the 'n'th vertex index array to the location pointed to
 *  by 'index_arr' and return the length of the array. The index array
 *  contains the vertices that make up the 'n'th point.
 *
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal point
 */
size_t obj_polygonal_v_point_vertices(obj_contents_t contents, size_t n,
				      const size_t (*index_arr[]));

/**
 *  Obtain the list of polygonal attributes for all polygonal lines only
 *  identifed by vertices in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes describing the
 *  polygonal attributes for the ith line.
 *
 *  Return value
 *  The total number of polygonal lines only identified by vertices
 */
size_t obj_polygonal_v_lines(obj_contents_t contents,
			     const size_t (*attindex_arr[]));

/**
 *  Obtain the vertex indices for the 'n'th polygonal line only identifed by
 *  vertices in 'contents'
 *
 *  Copy a pointer to the 'n'th vertex index array to the location pointed to
 *  by 'index_arr' and return the length of the array. The index array
 *  contains the vertices that make up the 'n'th line.
 *
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal line
 */
size_t obj_polygonal_v_line_vertices(obj_contents_t contents, size_t n,
				     const size_t (*index_arr[]));

/**
 *  Obtain the list of polygonal attributes for all textured polygonal lines
 *  in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes desribing the
 *  polygonal attributes for the ith line.
 *
 *  Return value
 *  The total number of textured polygonal line
 */
size_t obj_polygonal_tv_lines(obj_contents_t contents,
			      const size_t (*attindex_arr[]));

/**
 *  Obtain the textured coordinate and vertex indices for the 'n'th textured
 *  polygonal line in 'contents'
 *
 *  Copy a pointer to the 'n'th multidimensional index array to the location
 *  pointed to by 'index_arr' and return the length of the array. 
 *
 *  The line indices are contained in an array of type const size_t[][2] and
 *  must not be modified by the caller. The format of the second dimension is:
 *  {vertex_index, texture_coordinate_index}
 *  
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal line
 */
size_t obj_polygonal_tv_line_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2]);

/**
 *  Obtain the list of polygonal attributes for all polygonal faces only
 *  identifed by vertices in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes describing the
 *  polygonal attributes for the ith face.
 *
 *  Return value
 *  The total number of polygonal faces only identified by vertices
 */
size_t obj_polygonal_v_faces(obj_contents_t contents,
			     const size_t (*attindex_arr[]));

/**
 *  Obtain the vertex indices for the 'n'th polygonal face only identifed by
 *  vertices in 'contents'
 *
 *  Copy a pointer to the 'n'th vertex index array to the location pointed to
 *  by 'index_arr' and return the length of the array. The index array
 *  contains the vertices that make up the 'n'th face.
 *
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal face
 */
size_t obj_polygonal_v_face_vertices(obj_contents_t contents, size_t n,
				     const size_t (*index_arr[]));

/**
 *  Obtain the list of polygonal attributes for all textured polygonal faces
 *  in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes desribing the
 *  polygonal attributes for the ith face.
 *
 *  Return value
 *  The total number of textured polygonal faces
 */
size_t obj_polygonal_tv_faces(obj_contents_t contents,
			      const size_t (*attindex_arr[]));

/**
 *  Obtain the textured coordinate and vertex indices for the 'n'th textured
 *  polygonal face in 'contents'
 *
 *  Copy a pointer to the 'n'th multidimensional index array to the location
 *  pointed to by 'index_arr' and return the length of the array. 
 *
 *  The face indices are contained in an array of type const size_t[][2] and
 *  must not be modified by the caller. The format of the second dimension is:
 *  {vertex_index, texture_coordinate_index}
 *  
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal face
 */
size_t obj_polygonal_tv_face_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2]);

/**
 *  Obtain the list of polygonal attributes for all oriented polygonal faces
 *  in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes desribing the
 *  polygonal attributes for the ith face.
 *
 *  Return value
 *  The total number of oriented polygonal faces
 */
size_t obj_polygonal_nv_faces(obj_contents_t contents,
			      const size_t (*attindex_arr[]));

/**
 *  Obtain the normal and vertex indices for the 'n'th oriented polygonal face
 *  in 'contents'
 *
 *  Copy a pointer to the 'n'th multidimensional index array to the location
 *  pointed to by 'index_arr' and return the length of the array. 
 *
 *  The face indices are contained in an array of type const size_t[][2] and
 *  must not be modified by the caller. The format of the second dimension is:
 *  {vertex_index, normal_index}
 *  
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal face
 */
size_t obj_polygonal_nv_face_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2]);

/**
 *  Obtain the list of polygonal attributes for all textured and oriented
 *  polygonal faces in 'contents'
 *
 *  Copy a pointer to an index array to the location pointed to by attindex_arr
 *  and return the list length.
 *
 *  The value of the ith element in 'attindex_arr' is the index into the
 *  'attr_list' obtained from obj_polygonal_attributes desribing the
 *  polygonal attributes for the ith face.
 *
 *  Return value
 *  The total number of textured oriented polygonal faces
 */
size_t obj_polygonal_tnv_faces(obj_contents_t contents,
			       const size_t (*attindex_arr[]));

/**
 *  Obtain the texture cordinate, normal and vertex indices for the 'n'th
 *  textured and oriented polygonal face in 'contents'
 *
 *  Copy a pointer to the 'n'th multidimensional index array to the location
 *  pointed to by 'index_arr' and return the length of the array. 
 *
 *  The face indices are contained in an array of type const size_t[][3] and
 *  must not be modified by the caller. The format of the second dimension is:
 *  {vertex_index, texture_coordinate_index, normal_index}
 *  
 *  Return value
 *  The number of vertices that make up the 'n'th polygonal face
 */
size_t obj_polygonal_tnv_face_vertices(obj_contents_t contents, size_t face,
				       const size_t (*index_arr[])[3]);

__END_DECLS

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
