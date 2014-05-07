#!/bin/sh

tables="\
bpl \
cmap \
commands \
fb \
hmkeys \
ids \
overlap \
plot \
screen \
scrollmenu \
shotlines \
submenu \
topmenu\
"

for table in $tables ; do
  tbl $table.tbl > $table.mm
done

