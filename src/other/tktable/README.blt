If tkTable is used at the same time as BLT then there are two name
conflicts to be aware of.

BLT also has a table.n man page.  TkTable's man page will still be
available as tkTable.n.

BLT also has a "table" command.  The table command of the last
extension loaded will be in effect.  If you need to use both table
commands then eval "rename table blttable" after loading blt and
before loading tkTable, or perhaps "rename table tkTable" if you
load the tkTable extension first.

In general this shouldn't be a problem as long as you load tkTable
last.  The BLT "table" command facilities have been subsumed by the
Tk "grid" command (available in Tk4.1+), so the BLT table should
only be used in legacy code.

Alternatively, if you want both or have another "table" command,
then change the TBL_COMMAND macro in the makefile before compiling,
and it tkTable will define your named command for the table widget.
