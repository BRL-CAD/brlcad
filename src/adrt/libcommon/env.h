#ifndef _COMMON_ENV_H
#define _COMMON_ENV_H

#include "adrt_common.h"

extern	void	common_env_init(common_env_t *env);
extern	void	common_env_free(common_env_t *env);
extern	void	common_env_prep(common_env_t *env);
extern	void	common_env_read(common_env_t *env);

#endif
