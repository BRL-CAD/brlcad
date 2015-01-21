#include "bu.h"
#include "gcv_private.h"

struct gcv_context {
  struct db_i *db_instance;
};

struct gcv_opts {
  void *p;
};

struct gcv_filter {
  const char *name;
  const struct gcv_opts *options;
  int for_reading;
  const struct gcv_plugin_info *plugin_info;
};



int
gcv_init(struct gcv_context *context)
{
  context->db_instance = db_create_inmem();
  return 0;
}


int
gcv_destroy(struct gcv_context *context)
{
  db_close(context->db_instance);
  return 0;
}


void
gcv_reader(struct gcv_filter *reader, const char *source, const struct gcv_opts *opts)
{
  if (!source) source = "-";

  if (!BU_STR_EQUAL(source, "-")) {
    bu_log("Scheduling read from %s\n", source);
  } else {
    bu_log("Scheduling read from STDIN\n");
  }

  reader->name = source;
  reader->options = opts;
  reader->for_reading = 1;
  reader->plugin_info = gcv_plugin_find(source, reader->for_reading);

  if (!reader->plugin_info)
    bu_log("No reader found for '%s'\n", source);
}


void
gcv_writer(struct gcv_filter *writer, const char *target, const struct gcv_opts *opts)
{
  if (!target) target = "-";

  if (!BU_STR_EQUAL(target, "-")) {
    bu_log("Scheduling write to %s\n", target);
  } else {
    bu_log("Scheduling write to STDOUT\n");
  }
  writer->name = target;
  writer->options = opts;
  writer->for_reading = 0;
  writer->plugin_info = gcv_plugin_find(target, writer->for_reading);

  if (!writer->plugin_info)
    bu_log("No writer found for '%s'\n", target);
}


int
gcv_execute(struct gcv_context *cxt, const struct gcv_filter *filter)
{
  if (!cxt || !filter)
    return 0;

  bu_log("Filtering %s\n", filter->name);

  if (filter->for_reading) {
    struct rt_wdb *wdbp = wdb_dbopen(cxt->db_instance, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);

    if (!wdbp) {
      bu_log("wdb_dbopen() failed\n");
      return -1;
    }

    if (!filter->plugin_info || !filter->plugin_info->reader_fn) {
      bu_log("Filter has no reader\n");
      return -1;
    }

    return !filter->plugin_info->reader_fn(filter->name, wdbp, filter->options);
  }

  if (!filter->plugin_info || !filter->plugin_info->writer_fn) {
    bu_log("Filter has no writer\n");
    return -1;
  }

  return !filter->plugin_info->writer_fn(filter->name, cxt->db_instance, filter->options);
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

  result = gcv_execute(&context, &reader);
  if (result)
    bu_exit(2, "ERROR: Unable to read input data from %s\n", in_file);

  result = gcv_execute(&context, &writer);
  if (result)
    bu_exit(2, "ERROR: Unable to write output data to %s\n", out_file);

  gcv_destroy(&context);

  return 0;
}


int
main(int ac, char **av)
{
  if (ac < 3
      || (ac == 2 && av[1][0] == '-' && av[1][1] == 'h')) {
    bu_log("Usage: %s [-h] [input[:options]] [output[:options]]\n", av[0]);
    return 1;
  }

  gcv_convert(av[1], NULL, av[2], NULL);

  return 0;
}
