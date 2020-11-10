include_directories(..)

sc_addexec( print_schemas "../fedex.c;print_schemas.c" "express;base" )
sc_addexec( print_attrs "../fedex.c;print_attrs.c" "express;base" )

