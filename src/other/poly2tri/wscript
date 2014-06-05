#!/usr/bin/env python
# encoding: utf-8
# waf 1.6.10

VERSION='0.3.3'
import sys
APPNAME='p2t'
top = '.'
out = 'build'

CPP_SOURCES = ['poly2tri/common/shapes.cc',
               'poly2tri/sweep/cdt.cc',
               'poly2tri/sweep/advancing_front.cc',
               'poly2tri/sweep/sweep_context.cc',
               'poly2tri/sweep/sweep.cc',
               'testbed/main.cc']

from waflib.Tools.compiler_cxx import cxx_compiler
cxx_compiler['win32'] = ['g++']

#Platform specific libs
if sys.platform == 'win32':
    # MS Windows
    sys_libs = ['glfw', 'opengl32']
elif sys.platform == 'darwin':
    # Apple OSX
    sys_libs = ['glfw']
else:
    # GNU/Linux, BSD, etc
    sys_libs = ['glfw', 'GL']

def options(opt):
  print('  set_options')
  opt.load('compiler_cxx')

def configure(conf):
  print('  calling the configuration')
  conf.load('compiler_cxx')
  conf.env.CXXFLAGS = ['-O3', '-ffast-math']
  conf.env.DEFINES_P2T = ['P2T']
  conf.env.LIB_P2T = sys_libs
  if sys.platform == 'darwin':
    conf.env.FRAMEWORK = ['OpenGL', 'Cocoa']

def build(bld):
  print('  building')
  bld.program(features = 'cxx cxxprogram', source=CPP_SOURCES, target = 'p2t', uselib = 'P2T')