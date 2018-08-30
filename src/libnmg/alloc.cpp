
static thread_local struct bu_pool *data = {NULL};


/* #define NMG_GETSTRUCT(p, str) p = nmg_alloc(sizeof(p)); */

void *
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

void
nmg_destroy()
{
  bu_pool_delete(data);
}
