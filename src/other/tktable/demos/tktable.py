#
# #### OUTDATE MODULE ####
# This has been superceded by the tktable.py that ships in the lib area.
# This is kept for compatibility as the newer wrapper is not 100% compatible.
# #### OUTDATE MODULE ####
#
# This file is taken from the usenet:
#    http://groups.google.com/groups?selm=351A52BC.27EA0BE2%40desys.de
#    From: Klaus Roethemeyer <klaus.roethemeyer at desys.de>
#
# It is provided here as an example of using Tktable with Python/Tkinter.

#============================================================================
#
# MODULE:	This module contains the wrapper class for the tktable widget
#
# CREATED:	Roethemeyer, 20.01.98
#
# VERSION:	$Id$
# 
#============================================================================

#============================================================================
# import modules
#----------------------------------------------------------------------------
import string, types, Tkinter
#----------------------------------------------------------------------------

#============================================================================
# ArrayVar
#----------------------------------------------------------------------------
class ArrayVar(Tkinter.Variable):
    _default = ''

    def __init__(self, master = None):
        Tkinter.Variable.__init__(self, master)

    def get(self, index = None):
        if not index:
            res = {}
            for i in self.names():
                res[i] = self._tk.globalgetvar(self._name, i)
            try: del res['None']
            except KeyError: pass
            return res
        else:
            return self._tk.globalgetvar(self._name, index)

    def names(self):
        return string.split(self._tk.call('array', 'names', self._name))

    def set(self, index, value = ''):
        if value == None:
            value = ''
        return self._tk.globalsetvar(self._name, index, value)
#----------------------------------------------------------------------------


#============================================================================
# Table
#----------------------------------------------------------------------------
class Table(Tkinter.Widget):

    _switches1 = ('cols', 'holddimensions', 'holdtags', 'keeptitles', 'rows', '-')
    _tabsubst_format = ('%c', '%C', '%i', '%r', '%s', '%S', '%W')
    _tabsubst_commands = ('browsecommand', 'browsecmd', 'command',
                          'selectioncommand', 'selcmd', 
                          'validatecommand', 'valcmd')

    def __init__(self, master, cnf={}, **kw):
        try:
            master.tk.call('package', 'require', 'Tktable')
        except Tkinter.TclError:
            master.tk.call('load', '', 'Tktable')
        Tkinter.Widget.__init__(self, master, 'table', cnf, kw)

    def _options(self, cnf, kw = None):
        if kw:
            cnf = Tkinter._cnfmerge((cnf, kw))
        else:
            cnf = Tkinter._cnfmerge(cnf)
        res = ()
        for k, v in cnf.items():
            if v is not None:
                if k[-1] == '_': k = k[:-1]
                if callable(v):
                    if k in self._tabsubst_commands:
                        v = "%s %s"  % (self._register(v, self._tabsubst),
                                        string.join(self._tabsubst_format))
                    else:
                        v = self._register(v)
                res = res + ('-'+k, v)
        return res

    def _tabsubst(self, *args):
        tk = self.tk
        if len(args) != len(self._tabsubst_format): return args
        c, C, i, r, s, S, W = args
        e = Tkinter.Event()
        e.widget = self
        e.c = tk.getint(c)
        e.i = tk.getint(i)
        e.r = tk.getint(r)
        e.C = (e.r, e.c)
        try: e.s = tk.getint(s)
        except Tkinter.TclError: e.s = s
        try: e.S = tk.getint(S)
        except Tkinter.TclError: e.S = S
        e.W = W
        return (e,)


    def _getCells(self, cellString):
        res = []
        for i in string.split(cellString):
            res.append(tuple(map(int, string.split(i, ','))))
        return res

    def _getLines(self, lineString):
        return map(int, string.split(lineString))

    def _prepareArgs1(self, args):
        args = list(args)

        for i in xrange(len(args)):
            if args[i] in self._switches1:
                args[i] = "-" + args[i]

        return tuple(args)

        
    def activate(self, index):
        self.tk.call(self._w, 'activate', index)

    def bbox(self, first, last=None):
        return self._getints(self.tk.call(self._w, 'bbox', first, last)) or None

    def border_mark(self, x, y, row=None, col=None):
        self.tk.call(self._w, 'border', 'mark', x, y, row, col)

    def border_dragto(self, x, y):
        self.tk.call(self._w, 'border', 'dragto', x, y)

    def curselection(self, setValue = None):
        if setValue != None:
            self.tk.call(self._w, 'curselection', 'set', setValue)

        else:
            return self._getCells(self.tk.call(self._w, 'curselection'))

    def delete_active(self, index, more = None):
        self.tk.call(self._w, 'delete', 'active', index, more)

    def delete_cols(self, *args):
        apply(self.tk.call, (self._w, 'delete', 'cols') + self._prepareArgs1(args))

    def delete_rows(self, *args):
        apply(self.tk.call, (self._w, 'delete', 'rows') + self._prepareArgs1(args))

    def flush(self, first=None, last=None):
        self.tk.call(self._w, 'flush', first, last)

    def get(self, first, last=None):
        return self.tk.call(self._w, 'get', first, last)

    def height(self, *args):
        apply(self.tk.call, (self._w, 'height') + args)
        
    def icursor(self, arg):
        self.tk.call(self._w, 'icursor', arg)

    def index(self, index, rc = None):
        if rc == None:
            return self._getCells(self.tk.call(self._w, 'index', index, rc))[0]
        else:
            return self._getCells(self.tk.call(self._w, 'index', index, rc))[0][0]
            
    def insert_active(self, index, value):
        self.tk.call(self._w, 'insert', 'active', index, value)

    def insert_cols(self, *args):
        apply(self.tk.call, (self._w, 'insert', 'cols') + self._prepareArgs1(args))

    def insert_rows(self, *args):
        apply(self.tk.call, (self._w, 'insert', 'rows') + self._prepareArgs1(args))

    def reread(self):
        self.tk.call(self._w, 'reread')

    def scan_mark(self, x, y):
        self.tk.call(self._w, 'scan', 'mark', x, y)

    def scan_dragto(self, x, y):
        self.tk.call(self._w, 'scan', 'dragto', x, y)

    def see(self, index):
        self.tk.call(self._w, 'see', index)

    def selection_anchor(self, index):
        self.tk.call(self._w, 'selection', 'anchor', index)

    def selection_clear(self, first, last=None):
        self.tk.call(self._w, 'selection', 'clear', first, last)

    def selection_includes(self, index):
        return int(self.tk.call(self._w, 'selection', 'includes', index))

    def selection_set(self, first, last=None):
        self.tk.call(self._w, 'selection', 'set', first, last)

    def set(self, *args):
        apply(self.tk.call, (self._w, 'set') + args)

    def tag_cell(self, tagName, *args):
        result = apply(self.tk.call, (self._w, 'tag', 'cell', tagName) + args)
        if not args: return self._getCells(result)

    def tag_cget(self, tagName, option):
        return self.tk.call(self._w, 'tag', 'cget', tagName, '-' + option)

    def tag_col(self, tagName, *args):
        result = apply(self.tk.call, (self._w, 'tag', 'col', tagName) + args)
        if not args: return self._getLines(result)

    def tag_configure(self, tagName, cnf={}, **kw):
        if not cnf and not kw:
            return self.tk.call(self._w, 'tag', 'configure', tagName)
        if type(cnf) == types.StringType and not kw:
            return self.tk.call(self._w, 'tag', 'configure', tagName, '-' + cnf)
        if type(cnf) == types.DictType:
            apply(self.tk.call,
                  (self._w, 'tag', 'configure', tagName) 
                  + self._options(cnf, kw))
        else:
            raise TypeError, "usage: <instance>.tag_configure tagName [option] | [option=value]+"
            
    def tag_delete(self, tagName):
        self.tk.call(self._w, 'tag', 'delete', tagName)

    def tag_exists(self, tagName):
        return self.getboolean(self.tk.call(self._w, 'tag', 'exists', tagName))

    def tag_includes(self, tagName, index):
        return self.getboolean(self.tk.call(self._w, 'tag', 'includes', tagName, index))

    def tag_names(self, pattern=None):
        return self.tk.call(self._w, 'tag', 'names', pattern)

    def tag_row(self, tagName, *args):
        result = apply(self.tk.call, (self._w, 'tag', 'row', tagName) + args)
        if not args: return self._getLines(result)

    def validate(self, index):
        self.tk.call(self._w, 'validate', index)

    def width(self, *args):
        result = apply(self.tk.call, (self._w, 'width') + args)
        if not args: 
            str = string.replace(result, '{', '')
            str = string.replace(str, '}', '')
            lst = string.split(str)
            x = len(lst)
            x2 = x / 2
            return tuple(map(lambda i, j, l=lst: (int(l[i]), int(l[j])),
                             xrange(x2), xrange(x2, x)))
        elif len(args) == 1:
            return int(result)
        else:
            return result
        
    def xview(self, *args):
        if not args:
            return self._getdoubles(self.tk.call(self._w, 'xview'))
        apply(self.tk.call, (self._w, 'xview') + args)

    def yview(self, *args):
        if not args:
            return self._getdoubles(self.tk.call(self._w, 'yview'))
        apply(self.tk.call, (self._w, 'yview') + args)
#----------------------------------------------------------------------------


#============================================================================
# Test-Function
#----------------------------------------------------------------------------
if __name__ == '__main__':
    from Tkinter import Tk, Label, Button
    import pprint

    prn = pprint.PrettyPrinter(indent = 6).pprint

    def test_cmd(event=None):
        if event.i == 0:
            return '%i, %i' % (event.r, event.c)
        else:
            return 'set'


    def browsecmd(event):
        print "event:", event.__dict__
        print "curselection:", test.curselection()
        print "active:", test.index('active', 'row')
        print "anchor:", test.index('anchor', 'row')

    root = Tk()
    #root.tk.call('load', '', 'Tktable')

    var = ArrayVar(root)
    for y in range(-1, 4):
        for x in range(-1, 5):
            index = "%i,%i" % (y, x)
            var.set(index, index)

    label = Label(root, text="Proof-of-existence test for Tktable")
    label.pack(side = 'top', fill = 'x')
    
    quit = Button(root, text="QUIT", command=root.destroy)
    quit.pack(side = 'bottom', fill = 'x')

    test = Table(root,
                 rows=10,
                 cols=5,
                 state='disabled',
                 width=6,
                 height=6,
                 titlerows=1,
                 titlecols=1,
                 roworigin=-1,
                 colorigin=-1,
                 selectmode='browse',
                 selecttype='row',
                 rowstretch='unset',
                 colstretch='last',
                 browsecmd=browsecmd,
                 flashmode='on',
                 variable=var,
                 usecommand=0,
                 command=test_cmd)
    test.pack(expand=1, fill='both')
    test.tag_configure('sel', background = 'yellow')
    test.tag_configure('active', background = 'blue')
    test.tag_configure('title', anchor='w', bg='red', relief='sunken')
    root.mainloop()
#----------------------------------------------------------------------------
