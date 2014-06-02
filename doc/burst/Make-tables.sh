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
  gtbl $table.tbl > $table.mm
done

