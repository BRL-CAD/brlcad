# R E N D E R _ T W I N _ S T R U C T U R E . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#

cmake_minimum_required(VERSION 3.22)
set(model_name twin)
set(example_name twin-structure)
if(NOT DEFINED SOURCE_DB)
  set(SOURCE_DB "${BRLCAD_PREFIX}/share/db/faa/Generic_Twin.g")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/render_common.cmake")
render_engineering_views()
