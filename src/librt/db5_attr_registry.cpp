/*             D B 5 _ A T T R _ R E G I S T R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include <map>
#include <string>

#include "raytrace.h"

#ifdef __cplusplus
extern "C" {
#endif
    void db5_attr_registry_init(struct db5_registry *registry);
    void db5_attr_registry_free(struct db5_registry *registry);
    int db5_attr_create(struct db5_registry *registry,
	    int attr_type,
	    int is_binary,
	    int attr_subtype,
	    const char *name,
	    const char *description,
	    const char *examples,
	    const char *aliases,
	    const char *property,
	    const char *long_description);
    int db5_attr_register(struct db5_registry *registry, struct db5_attr_ctype *attribute);
    int db5_attr_deregister(struct db5_registry *registry, const char *name);
    struct db5_attr_ctype *db5_attr_get(struct db5_registry *registry, const char *name);
    struct db5_attr_ctype **db5_attr_dump(struct db5_registry *registry);
#ifdef __cplusplus
}
#endif

#define attr_regmap_t std::map<std::string, struct db5_attr_ctype *>
#define attr_regmap_it std::map<std::string, struct db5_attr_ctype *>::iterator

void db5_attr_registry_init(struct db5_registry *registry)
{
    if (!registry) return;
    registry->internal = (void *)new attr_regmap_t;
}

HIDDEN
void db5_attr_ctype_free(struct db5_attr_ctype *attr)
{
    bu_free((void *)attr->name, "free attr name");
    if (attr->description)  bu_free((void *)attr->description, "free attr description");
    if (attr->examples)  bu_free((void *)attr->examples, "free attr examples");
    if (attr->aliases)  bu_free((void *)attr->aliases, "free attr aliases");
    if (attr->property)  bu_free((void *)attr->property, "free attr property");
    if (attr->long_description) bu_free((void *)attr->long_description, "free attr long_description");
    bu_free(attr, "free registered attribute");
}

void
db5_attr_registry_free(struct db5_registry *registry)
{
    if (!registry) return;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    attr_regmap_it it;
    for(it = regmap->begin(); it != regmap->end(); it++) {
	struct db5_attr_ctype *attr = (struct db5_attr_ctype *)(*it).second;
	db5_attr_ctype_free(attr);
    }
    regmap->clear();
}

int
db5_attr_create(struct db5_registry *registry,
	int attr_type,
	int is_binary,
	int attr_subtype,
	const char *name,
	const char *description,
	const char *examples,
	const char *aliases,
	const char *property,
	const char *long_description)
{
    if (!registry) return -1;
    if (!name || strlen(name) == 0) return -1;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    if (regmap->find(std::string(name)) != regmap->end()) return -1;
    struct db5_attr_ctype *new_attr = (struct db5_attr_ctype *)bu_calloc(1, sizeof(struct db5_attr_ctype), "new attribute");
    new_attr->attr_type = attr_type;
    new_attr->is_binary = is_binary;
    new_attr->attr_subtype = attr_subtype;
    new_attr->name = bu_strdup(name);
    if (description) new_attr->description = bu_strdup(description);
    if (examples) new_attr->examples = bu_strdup(examples);
    if (aliases) new_attr->aliases = bu_strdup(aliases);
    if (property) new_attr->property = bu_strdup(property);
    if (long_description) new_attr->long_description = bu_strdup(long_description);
    (*regmap)[std::string(name)] = new_attr;
    return 0;
}


int
db5_attr_register(struct db5_registry *registry, struct db5_attr_ctype *attribute)
{
    if (!registry) return -1;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    attr_regmap_it it = regmap->find(std::string(attribute->name));
    if (it == regmap->end()) {
	(*regmap)[std::string(attribute->name)] = attribute;
	return 0;
    }
    return 1;
}

int
db5_attr_deregister(struct db5_registry *registry, const char *name)
{
    if (!registry) return -1;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    attr_regmap_it it = regmap->find(std::string(name));
    if (it != regmap->end()) {
	struct db5_attr_ctype *attr = (struct db5_attr_ctype *)(*it).second;
	db5_attr_ctype_free(attr);
	regmap->erase(it);
	return 0;
    }
    return 1;
}


struct db5_attr_ctype *
db5_attr_get(struct db5_registry *registry, const char *name)
{
    if (!registry) return NULL;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    attr_regmap_it it = regmap->find(std::string(name));
    if (it != regmap->end()) {
	return (struct db5_attr_ctype *)(*it).second;
    }
    return NULL;
}

struct db5_attr_ctype **
db5_attr_dump(struct db5_registry *registry)
{
    int i = 0;
    if (!registry) return NULL;
    attr_regmap_t *regmap = static_cast<attr_regmap_t *>(registry->internal);
    if (!regmap->size()) return NULL;
    attr_regmap_it it;
    struct db5_attr_ctype **attr_array = (struct db5_attr_ctype **)bu_calloc(regmap->size() + 1, sizeof(struct db5_attr_ctype *), "attribute pointer list");
    for(it = regmap->begin(); it != regmap->end(); it++) {
	struct db5_attr_ctype *attr = (struct db5_attr_ctype *)(*it).second;
	attr_array[i] = attr;
	i++;
    }
    return attr_array;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

