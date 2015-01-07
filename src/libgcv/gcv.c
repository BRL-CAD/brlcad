#include "bu.h"
#include "gcv.h"

struct gcv_context {
  int i;
};

struct gcv_opts {
  void *p;
};

struct gcv_filter {
  const char *name;
};



int
gcv_init(struct gcv_context *UNUSED(context))
{
  return 0;
}


int
gcv_destroy(struct gcv_context *UNUSED(context))
{
  return 0;
}


struct gcv_filter *
gcv_filter(struct gcv_context *cxt, const char *file, const struct gcv_opts *UNUSED(opts))
{
  struct gcv_filter *filter = NULL;

  if (cxt) {
    filter = bu_calloc(sizeof(struct gcv_filter), 1, "alloc filter");
    filter->name = file;
  }

  return filter;
}


int
gcv_reader(struct gcv_context *cxt, const struct gcv_filter *reader)
{
  if (cxt)
    bu_log("Reading from %s\n", reader->name);
  return 0;
}


int
gcv_writer(struct gcv_context *cxt, const struct gcv_filter *writer)
{
  if (cxt)
    bu_log("Writing to %s\n", writer->name);
  return 0;
}


int
gcv_convert(const char *in_file, const struct gcv_opts *in_opts, const char *out_file, const struct gcv_opts *out_opts)
{
  struct gcv_context context;
  struct gcv_filter *reader;
  struct gcv_filter *writer;

  gcv_init(&context);

  reader = gcv_filter(&context, in_file, in_opts);
  if (!reader)
    return -1;
  gcv_reader(&context, reader);

  writer = gcv_filter(&context, out_file, out_opts);
  if (!writer)
    return -2;
  gcv_writer(&context, writer);

  gcv_destroy(&context);

  return 0;
}


int
main(int ac, char **av)
{
  if (ac < 3
      || (ac < 2 && ac > 1 && av[1][0] == '-' && av[1][1] == 'h')) {
    bu_log("Usage: %s [-h] [input[:options]] [output[:options]]\n", av[0]);
    return 1;
  }

  gcv_convert(av[1], NULL, av[2], NULL);

  return 0;
}
