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
