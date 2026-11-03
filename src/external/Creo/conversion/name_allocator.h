/**
 *             N A M E _ A L L O C A T O R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_CONVERSION_NAME_ALLOCATOR_H
#define CREO_CONVERSION_NAME_ALLOCATOR_H

#include <string>
#include <vector>

#include "shared/conversion_snapshot.h"


struct db_i;


std::string creo_brl_snapshot_name_source(
    const std::string& rename_parameters,
    const std::string& model_name,
    const std::vector<struct snapshot_named_value>& parameters);

std::string creo_brl_snapshot_name_root(const std::string& source_name);

int creo_brl_snapshot_unique_name(struct db_i *database,
                                  const std::string& name_root,
                                  const std::vector<std::string>& reserved_names,
                                  std::string *name);

int creo_brl_snapshot_object_names(struct db_i *database,
                                   const std::string& name_root,
                                   const char *solid_suffix,
                                   std::string *solid_name,
                                   std::string *region_name);


#endif /* CREO_CONVERSION_NAME_ALLOCATOR_H */
