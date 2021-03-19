#!/usr/bin/python3

# to use in your gdb session
# pi (enters the interactive python interpreter)
# import gdb
# f = open(r'../../ybreaks.txt')
# ybreaks = [gdb.Breakpoint(x.strip()) for x in f]
# 
# note you'll also need to set the gdb source path to include the location of the grammar
# dir ../src/express
# dir ../src/express/generated
#

import re

y_line_re = re.compile(r'^#line\s(?P<lineno>[0-9]+)\s"expparse.y"')

with open(r'generated/expparse.c') as f, open(r'generated/ybreaks.txt', 'w+') as g:
    ybreaks = ['expparse.y:%s' % y_line_re.match(l).group('lineno') for l in f if y_line_re.match(l)]
    g.writelines(x + '\n' for x in ybreaks)
