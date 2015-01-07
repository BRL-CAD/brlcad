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


void
gcv_reader(struct gcv_filter *reader, const char *source, const struct gcv_opts *UNUSED(opts))
{
  if (source && !BU_STR_EQUAL(source, "-")) {
    bu_log("Scheduling read from %s\n", source);
  } else {
    bu_log("Scheduling read from STDIN\n");
  }
  reader->name = source;
}


void
gcv_writer(struct gcv_filter *writer, const char *target, const struct gcv_opts *UNUSED(opts))
{
  if (target && !BU_STR_EQUAL(target, "-")) {
    bu_log("Scheduling write to %s\n", target);
  } else {
    bu_log("Scheduling write to STDOUT\n");
  }
  writer->name = target;
}


int
gcv_filter(struct gcv_context *cxt, const struct gcv_filter *filter)
{
  if (!cxt || !filter)
    return 0;

  bu_log("Filtering %s\n", filter->name);

  return 0;
}


int
gcv_convert(const char *in_file, const struct gcv_opts *in_opts, const char *out_file, const struct gcv_opts *out_opts)
{
  struct gcv_context context;
  struct gcv_filter reader;
  struct gcv_filter writer;
  int result;

  gcv_init(&context);

  gcv_reader(&reader, in_file, in_opts);
  gcv_writer(&writer, out_file, out_opts);

  result = gcv_filter(&context, &reader);
  if (result)
    bu_exit(2, "ERROR: Unable to read input data from %s\n", in_file);

  result = gcv_filter(&context, &writer);
  if (result)
    bu_exit(2, "ERROR: Unable to write output data to %s\n", out_file);

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
