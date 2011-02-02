
"""
Tkinter support for the Togl 2.X Tk OpenGL widget.

Copyright (C) 2006-2007  Greg Couch
See the LICENSE file for copyright details.
"""
__all__ = ['Togl', 'NORMAL', 'OVERLAY']

import Tkinter
import weakref, atexit

# Overlay constants
NORMAL = 1
OVERLAY = 2

class Togl(Tkinter.Widget):
	"""Tk OpenGL Widget"""
	_instances = weakref.WeakKeyDictionary()

	def __init__(self, master=None, cnf={}, **kw):
		"""Return new Togl widget"""
		if master is None:
			master = Tkinter._default_root
		master.tk.call('package', 'require', 'Togl', '2.0')
		try:
			Tkinter.Widget.__init__(self, master, "togl", cnf, kw)
		except:
			Tkinter.Widget.destroy(self)
			raise
		Togl._instances[self] = True

	def _cbsubst(self, *args):
		"""callback command argument substitution"""
		if len(args) != 1:
			return args
		return (self._nametowidget(args[0]),)

	def _options(self, cnf, kw = None):
		"""Internal function."""
		if kw:
			cnf = Tkinter._cnfmerge((cnf, kw))
		else:
			cnf = Tkinter._cnfmerge(cnf)
		res = ()
		for k, v in cnf.items():
			if v is not None:
				if k[-1] == '_': k = k[:-1]
				if callable(v):
					if k.endswith('command'):
						v = self._register(v, self._cbsubst)
					else:
						v = self._register(v)
				res = res + ('-'+k, v)
		return res

	# cget, configure are inherited

	def extensions(self):
		"""Return list of supported OpenGL extensions"""
		return self.tk.call(self._w, 'extensions')

	def postredisplay(self):
		"""Cause the displaycommand callback to be called
		the next time the event loop is idle."""
		self.tk.call(self._w, 'postredisplay')

	def render(self):
		"""Call the displaycommand callback immediately."""
		self.tk.call(self._w, 'render')

	def swapbuffers(self):
		"""If single-buffred, just flush OpenGL command buffer.  If
		double-buffered, swap front and back buffers.  (So this is
		appropriate to call after every frame is drawn.)"""
		self.tk.call(self._w, 'swapbuffers')

	def makecurrent(self):
		"""Make widget the current OpenGL context"""
		self.tk.call(self._w, 'makecurrent')

	def takephoto(self, imageName):
		"""Copy current contents of widget into the given photo image
		"""
		self.tk.call(self._w, 'takephoto', imageName)

	def loadbitmapfont(self, name):
		return self.tk.call(self._w, 'loadbitmapfont', name)

	def unloadbitmapfont(self, fontbase):
		self.tk.call(self._w, 'unloadbitmapfont', fontbase)

	def uselayer(self, layer):
		self.tk.call(self._w, 'uselayer', layer)

	def showoverlay(self):
		self.tk.call(self._w, 'showoverlay')

	def hideoverlay(self):
		self.tk.call(self._w, 'hideoverlay')

	def postredisplayoverlay(self):
		self.tk.call(self._w, 'postredisplayoverlay')

	def renderoverlay(self):
		self.tk.call(self._w, 'renderoverlay')

	def existsoverlay(self):
		return self.tk.call(self._w, 'existsoverlay')

	def ismappedoverlay(self):
		return self.tk.call(self._w, 'ismappedoverlay')

	def getoverlaytransparentvalue(self):
		return self.tk.call(self._w, 'getoverlaytransparentvalue')

	def destroy(self):
		del Togl._instances[self]
		Tkinter.Widget.destroy(self)

def _cleanup():
	# destroy OpenGL contexts early, so destroycommand's don't
	# try to make any OpenGL calls during exit.
	for t in Togl._instances.keys():
		try:
			t.destroy()
		except Tkinter.TclError:
			pass
atexit.register(_cleanup)
