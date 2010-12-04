/* 
* Copyright (c) 1995-2010 United States Government as represented by
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

#include "obj_parser.h"

#include <errno.h>

#include <iostream>

void print_vertices(std::ostream &out, obj_contents_t contents)
{
  out << "vertices {\n";
  
  const float (*vert_list)[4];
  size_t len = obj_vertices(contents,&vert_list);
  for(size_t i=0; i<len; ++i) {
    out << "  <"
      << vert_list[i][0] << ","
      << vert_list[i][1] << ","
      << vert_list[i][2] << ","
      << vert_list[i][3] << ">\n";
  }
  
  out << "}\n";
}

void print_texture_coords(std::ostream &out, obj_contents_t contents)
{
  out << "texture coordinates {\n";
  
  const float (*coord_list)[3];
  size_t len = obj_texture_coord(contents,&coord_list);
  for(size_t i=0; i<len; ++i) {
    out << "  <"
      << coord_list[i][0] << ","
      << coord_list[i][1] << ","
      << coord_list[i][2] << ">\n";
  }
  
  out << "}\n";
}

void print_normals(std::ostream &out, obj_contents_t contents)
{
  out << "normals {\n";
  
  const float (*norm_list)[3];
  size_t len = obj_normals(contents,&norm_list);
  for(size_t i=0; i<len; ++i) {
    out << "  <"
      << norm_list[i][0] << ","
      << norm_list[i][1] << ","
      << norm_list[i][2] << ">\n";
  }
  
  out << "}\n";
}

void print_attributes(std::ostream &out, obj_contents_t contents,
  const obj_polygonal_attributes_t &face_attr)
{
  size_t size;
  size_t setsize;
  const char * const *str_arr;
  const size_t *indexset_arr;

  // print group set
  out << "  groups:";
  size = obj_groups(contents,&str_arr);
  if(size == 0 || face_attr.groupset_index >= obj_num_groupsets(contents)) {
    std::cerr << "Corrupted groupset list. num groups:" << size
      << " requested index: " << obj_num_groupsets(contents) << "\n";
    abort();
  }

  setsize = obj_groupset(contents,face_attr.groupset_index,&indexset_arr);
  for(size_t j=0; j<setsize; ++j) {
    if(indexset_arr[j] >= size) {
      std::cerr << "Corrupted grouplist list\n";
      abort();
    }
    out << " " << str_arr[indexset_arr[j]];
  }
  out << "\n";

  // print objects
  size = obj_objects(contents,&str_arr);
  if(size == 0 || face_attr.object_index >= size) {
    std::cerr << "Corrupted object list\n";
    abort();
  }
  out << "  object: '" << str_arr[face_attr.object_index] << "'\n";

  // print material
  size = obj_materials(contents,&str_arr);
  if(size == 0 || face_attr.material_index >= size) {
    std::cerr << "Corrupted material list\n";
    abort();
  }
  out << "  material: '" << str_arr[face_attr.material_index] << "'\n";

  // print materiallib set
  out << "  materiallibs:";
  size = obj_materiallibs(contents,&str_arr);
  if(size == 0 || face_attr.materiallibset_index >= obj_num_materiallibsets(contents)) {
    std::cerr << "Corrupted materiallibset list. num materiallibs:" << size
      << " requested index: " << obj_num_materiallibsets(contents) << "\n";
    abort();
  }

  setsize = obj_materiallibset(contents,face_attr.materiallibset_index,&indexset_arr);
  for(size_t j=0; j<setsize; ++j) {
    if(indexset_arr[j] >= size) {
      std::cerr << "Corrupted materialliblist list\n";
      abort();
    }
    out << " " << str_arr[indexset_arr[j]];
  }
  out << "\n";

  // print texmap
  size = obj_texmaps(contents,&str_arr);
  if(size == 0 || face_attr.texmap_index >= size) {
    std::cerr << "Corrupted textmap list\n";
    abort();
  }
  out << "  texmap: '" << str_arr[face_attr.texmap_index] << "'\n";

  // print texmaplibs set
  out << "  texmaplibs:";
  size = obj_texmaplibs(contents,&str_arr);
  if(size == 0 || face_attr.texmaplibset_index >= obj_num_texmaplibsets(contents)) {
    std::cerr << "Corrupted texmaplibset list. num texmaplibs:" << size
      << " requested index: " << obj_num_texmaplibsets(contents) << "\n";
    abort();
  }

  setsize = obj_texmaplibset(contents,face_attr.texmaplibset_index,&indexset_arr);
  for(size_t j=0; j<setsize; ++j) {
    if(indexset_arr[j] >= size) {
      std::cerr << "Corrupted texmapliblist list\n";
      abort();
    }
    out << " " << str_arr[indexset_arr[j]];
  }
  out << "\n";

  // print shadow objects
  size = obj_shadow_objs(contents,&str_arr);
  if(size == 0 || face_attr.shadow_obj_index >= size) {
    std::cerr << "Corrupted shadow object list\n";
    abort();
  }
  out << "  shadow_obj: '" << str_arr[face_attr.shadow_obj_index] << "'\n";

  // print trace objects
  size = obj_trace_objs(contents,&str_arr);
  if(size == 0 || face_attr.trace_obj_index >= size) {
    std::cerr << "Corrupted trace object list\n";
    abort();
  }
  out << "  trace_obj: '" << str_arr[face_attr.trace_obj_index] << "'\n";
  
  // print smooth group
  out << "  smoothing group: '" << face_attr.smooth_group << "'\n";

  // print bevel
  out << "  bevel: '" << bool(face_attr.bevel) << "'\n";

  // print c_interp
  out << "  c_interp: '" << bool(face_attr.c_interp) << "'\n";

  // print d_interp
  out << "  d_interp: '" << bool(face_attr.d_interp) << "'\n";

  // print lod
  if(face_attr.lod > 100) {
    std::cerr << "lod value too high: " << face_attr.lod << "\n";
    abort();
  }
  out << "  lod: '" << (unsigned int)(face_attr.lod) << "'\n";
}

void report_error(const char *in, int parse_err, obj_parser_t parser)
{
  std::cerr << "content_check: " << in << "\n";

  if(parse_err < 0)
    std::cerr << "SYNTAX ERROR: " << obj_parse_error(parser) << "\n";
  else
    std::cerr << "PARSER ERROR: " << strerror(parse_err) << "\n";
}

void print_contents(obj_contents_t contents)
{
  #ifdef DEBUG
    std::ostream &out = std::cerr;
  #else
    std::ostream &out = std::cout;
  #endif

  print_vertices(out,contents);
  print_texture_coords(out,contents);
  print_normals(out,contents);

  const obj_polygonal_attributes_t *polyattr_list;
  obj_polygonal_attributes(contents,&polyattr_list);

  size_t len;
  const size_t *attindex_arr;

  len = obj_polygonal_v_points(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "point {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t *index_arr;
    size_t size = obj_polygonal_v_point_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_v_lines(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "line {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t *index_arr;
    size_t size = obj_polygonal_v_line_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_tv_lines(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "textured line {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t (*index_arr)[2];
    size_t size = obj_polygonal_tv_line_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j][0]+1 << "/" << index_arr[j][1]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_v_faces(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "face {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t *index_arr;
    size_t size = obj_polygonal_v_face_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_tv_faces(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "textured face {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t (*index_arr)[2];
    size_t size = obj_polygonal_tv_face_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j][0]+1 << "/" << index_arr[j][1]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_nv_faces(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "oriented face {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t (*index_arr)[2];
    size_t size = obj_polygonal_nv_face_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j][0]+1 << "//" << index_arr[j][1]+1 << " ";
    out << "\n";

    out << "}\n";
  }

  len = obj_polygonal_tnv_faces(contents,&attindex_arr);
  for(size_t i=0; i<len; ++i) {
    out << "textured oriented face {\n";
    
    print_attributes(out,contents,polyattr_list[attindex_arr[i]]);

    out << "  vertices: ";
    const size_t (*index_arr)[3];
    size_t size = obj_polygonal_tnv_face_vertices(contents,i,&index_arr);
    for(size_t j=0; j<size; ++j)
      out << index_arr[j][0]+1 << "/" << index_arr[j][1]+1 
        << "/" << index_arr[j][1]+1 << " ";
    out << "\n";

    out << "}\n";
  }
}

/**
 *  Parse all files in argument list or from stdin if arg list is empty
 *  If any do not parse correctly, continue and report failure
 */
int main(int argc, char *argv[])
{
  obj_parser_t parser;
  obj_contents_t contents;

  int err = obj_parser_create(&parser);

  if(err) {
    std::cerr << "parser creation error\n";
    return err;
  }

  int parse_err;
  if(argc<2) {
    if((parse_err = obj_fparse(stdin,parser,&contents)))
      report_error("<stdin>",parse_err,parser);
    else
      print_contents(contents);
    
    err = err | parse_err;
  }
  else {
    while(*(++argv)) {
      if((parse_err = obj_parse(*argv,parser,&contents)))
        report_error(*argv,parse_err,parser);
      else
        print_contents(contents);

      err = err | parse_err;
    }
  }

  obj_parser_destroy(parser);

  return err;
}


