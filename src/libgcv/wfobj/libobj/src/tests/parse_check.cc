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

void report_error(const char *in, int parse_err, obj_parser_t parser)
{
  std::cerr << "parse_check: " << in << "\n";

  if(parse_err < 0)
    std::cerr << "SYNTAX ERROR: " << obj_parse_error(parser) << "\n";
  else
    std::cerr << "PARSER ERROR: " << strerror(parse_err) << "\n";
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
    err = err | parse_err;
  }
  else {
    while(*(++argv)) {
      if((parse_err = obj_parse(*argv,parser,&contents)))
        report_error(*argv,parse_err,parser);
      err = err | parse_err;
    }
  }

  obj_parser_destroy(parser);

  return err;
}


