/*                         C O N V 3 D M - G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file conv3dm-g.cpp
 *
 * Program to convert a Rhino model (in a .3dm file) to a BRL-CAD .g
 * file.
 *
 */


#ifndef CONV3DM_G_H
#define CONV3DM_G_H


#include <map>
#include <memory>
#include <string>
#include <vector>


class ONX_Model;
class ON_TextLog;
class ON_InstanceDefinition;
class ON_Brep;
class ON_3dmObjectAttributes;
class ON_Geometry;
class ON_InstanceRef;
class ON_Layer;
class ON_Material;
class ON_Bitmap;
struct rt_wdb;


namespace conv3dm
{


class RhinoConverter
{
public:
    RhinoConverter(const std::string &output_path);
    ~RhinoConverter();


    void write_model(const std::string &path, bool use_uuidnames,
		     bool random_colors);


private:
    class Color;
    struct ModelObject;

    RhinoConverter(const RhinoConverter &source);
    RhinoConverter &operator=(const RhinoConverter &source);

    void clean_model();
    void map_uuid_names();
    void create_all_bitmaps();
    void nest_all_layers();
    void create_all_layers();
    void create_all_idefs();
    void create_all_geometry();

    void create_geometry(const ON_Geometry *pGeometry,
			 const ON_3dmObjectAttributes &obj_attrs);

    void create_bitmap(const ON_Bitmap *bitmap);
    void create_layer(const ON_Layer &layer);
    void create_idef(const ON_InstanceDefinition &idef);

    void create_iref(const ON_InstanceRef &iref,
		     const ON_3dmObjectAttributes &iref_attrs);

    void create_brep(const ON_Brep &brep,
		     const ON_3dmObjectAttributes &brep_attrs);

    Color get_color(const ON_3dmObjectAttributes &obj_attrs) const;

    std::pair<std::string, std::string>
    get_shader(int index) const;

    bool is_name_taken(const std::string &name) const;

    std::string unique_name(const std::string &name,
			    const std::string &suffix) const;



    bool m_use_uuidnames;
    bool m_random_colors;
    std::string m_output_dirname;
    std::map<std::string, ModelObject> m_obj_map;
    std::auto_ptr<ON_TextLog> m_log;
    std::auto_ptr<ONX_Model> m_model;
    rt_wdb *m_db;
};


}


#endif




/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
