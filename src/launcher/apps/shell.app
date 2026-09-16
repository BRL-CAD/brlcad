# BRL-CAD launcher application manifest
#
# The special "@shell" exec value opens an interactive command shell with the
# BRL-CAD tools on the PATH (see launch.c); it is not a discovered executable.
name        = Command Shell
exec        = @shell
description = Shell with BRL-CAD tools
category    = Tools
order       = 90
console     = 1
