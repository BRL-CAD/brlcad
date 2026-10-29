#ifndef CREO_CONVERSION_PROFILE_READER_H
#define CREO_CONVERSION_PROFILE_READER_H

struct creo_brl_frontend_api;

void creo_brl_set_profile_frontend_api(const struct creo_brl_frontend_api *api);
void creo_brl_load_profile_options(const char *profile_path,
                                   const char *material_directory);

#endif /* CREO_CONVERSION_PROFILE_READER_H */
