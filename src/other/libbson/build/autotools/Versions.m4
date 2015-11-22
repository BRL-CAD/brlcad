m4_define([bson_major_version], [1])
m4_define([bson_minor_version], [3])
m4_define([bson_micro_version], [0])
m4_define([bson_prerelease_version], [beta0])

m4_define(
    [bson_version],
    m4_ifset(
        [bson_prerelease_version],
        [bson_major_version.bson_minor_version.bson_micro_version-bson_prerelease_version],
        [bson_major_version.bson_minor_version.bson_micro_version]))

# bump up by 1 for every micro release with no API changes, otherwise
# set to 0. after release, bump up by 1
m4_define([bson_interface_age], [0])
m4_define([bson_binary_age], [m4_eval(100 * bson_minor_version + bson_micro_version)])

m4_define([lt_current], [m4_eval(100 * bson_minor_version + bson_micro_version - bson_interface_age)])
m4_define([lt_revision], [bson_interface_age])
m4_define([lt_age], [m4_eval(bson_binary_age - bson_interface_age)])
