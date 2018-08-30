#include "common.h"

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#include <cassert>

#include "bu/malloc.h"
#include "bu/parallel.h"

#if defined(HAVE_THREAD_LOCAL)
static thread_local struct bu_pool *data = {NULL};

#elif defined(HAVE___THREAD)

static __thread struct bu_pool *data = {NULL};

#elif defined(HAVE___DECLSPEC_THREAD)

static __declspec(thread) struct bu_pool *data = {NULL};

#endif

/* #define NMG_GETSTRUCT(p, _type) p = (struct _type *)nmg_alloc(sizeof(struct _type)) */

extern "C" void *
nmg_alloc(size_t size)
{
  assert(size < 2048);
  if (!data)
    data = bu_pool_create(2048);
  return bu_pool_alloc(data, 1, size);
}

void
nmg_free(void *)
{
  /* nada */
}

extern "C" void
nmg_destroy()
{
  bu_pool_delete(data);
}
