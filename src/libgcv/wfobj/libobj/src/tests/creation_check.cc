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

int main(int argc, char *argv[])
{
  obj_parser_t parser;

  int err = obj_parser_create(&parser);
  
  if(err)
    return err;
  
  obj_parser_destroy(parser);

  return err;
}


