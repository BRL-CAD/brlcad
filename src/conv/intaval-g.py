#!/usr/bin/env python
#
# usage:  intaval-g.py intaval_file mged_cmds
#
#

import sys
def mashup(l):
    if len(l) < 1:
        return ''

    s = str(l[0])
    for i in l[1:]:
        s += '_' + i
    return s

if len(sys.argv) < 3 or len(sys.argv) > 4:
    print 'usage:  %s intaval_input.geom mged_output.tcl [input.matl]' % (sys.argv[0])
    sys.exit()

# open the file
fd = open(sys.argv[1], 'r')

outfile = open(sys.argv[2], 'w')

component_db = { }
# if we got a component/material file, load it
if (len(sys.argv) > 3):
    matl = open(sys.argv[3], 'r')
    matl.readline()
    
    tok = matl.readline().split()
    while tok[0] != '-1' > 0:
        name = mashup(tok[3:])
        component_db[int(tok[0])] = [tok[1], tok[2], name]
        tok = matl.readline().split()
            
# uniquely number primitives
prim = 1

logfile = open('logfile.txt', 'w')
def dlog (s):
    logfile.write(s + '\n')
    logfile.flush()

# format:
# name [r, g, b] transp
material_names = {
int(0) : ['air', [1, 1, 1], 0.9],
int(1) : ['Aluminum', [0.7, 0.7, 0.6], 0],
int(2) : ['Steel_B100', [0.1, 0.6, 0.6], 0],
int(3) : ['Steel_B300', [0.6, 0.1, 0.7], 0],
int(4) :  ['Magnesium',  [0.7, 0.7, 0.7], 0],
int(5) : ['Titanium_alloy',  [0.7, 0.7, 0.7], 0],
int(6) : ['Cast_Iron', [0.3, 0.3, 0.3], 0],
int(7) : ['Face-hardened_steel',  [0.7, 0.7, 0.7], 0],
int(8) : ['Mild_homogeneous_steel',  [0.7, 0.7, 0.7], 0],
int(9) : ['Hard_homogeneous_steel',  [0.7, 0.7, 0.7], 0],
int(10) : ['Copper',  [0.5, 0.2, 0.2], 0],
int(11) : ['Lead',  [0.7, 0.7, 0.7], 0],
int(12) : ['Tuballoy',  [0.7, 0.7, 0.7], 0],
int(13) : ['Unbonded_nylon',  [0.8, 0.7, 0.7], 0],
int(14) : ['Bonded_nylon',  [0.7, 0.8, 0.7], 0],
int(15) : ['Lexan',  [0.7, 0.7, 0.7], 0.8],
int(16) : ['Cast_plexiglass',  [0.7, 0.7, 0.7], 0.8],
int(17) : ['Stretched_plexiglass',  [0.7, 0.7, 0.7], 0.8],
int(18) : ['Doron',  [0.7, 0.7, 0.7], 0],
int(19) : ['Bullet_resistant_glass',  [0.4, 0.4, 0.7], 0.7],
int(20) : ['Crew_members',  [0.7, 0.7, 0.2], 0],
int(21) : ['Fuel',  [0.1, 0.7, 0.1], 0.1],
int(22) : ['Water',  [0.1, 0.1, 0.7], 0.6],
int(23) : ['Oil',  [0.1, 0.1, 0.1], 0.1],
int(30) : ['Woven_roving',  [0.7, 0.7, 0.7], 0]
}

used_materials = []
used_components = []

#
# Look up the material/component for the object and assign
#
def assign_material(name, id):
    global used_materials
    global used_components 

    if component_db.has_key(id):
        comp = component_db[id]

        outfile.write('g %s %s\n' % (comp[2], name))
        if comp[2] not in used_components:
            used_components.append(comp[2])


        if material_names.has_key(int(comp[0])):
            mat_type = material_names[int(comp[0])]

            outfile.write('shader %s {plastic {tr %g}}\n' % (name, mat_type[2]))
            
            args = [name] + [int(x*255.0) for x in mat_type[1]]
            outfile.write('adjust %s rgb { %d %d %d }\n' % tuple(args))
            outfile.write('g %s %s\n' % (mat_type[0], name))

            if mat_type[0] not in used_materials:
                used_materials.append(mat_type[0])
        else:
            mat_type = '%s.g' % comp[0]
            outfile.write('g %s %s\n' % (mat_type, name))
            if mat_type not in used_materials:
                used_materials.append(mat_type)
    else:
        comp = '%d.g' % id
        outfile.write('g %s %s\n' % (comp, name))
        if comp not in used_components:
            used_components.append(comp)

#
#  Wish we had examples of this to develop with
#
def long_form(v, m, s, fd):
    print 'long_form not implemented'
    for i in xrange(0, v):
        tup = [float(x) for x in fd.readline().split()]


#
# An stl-like object
#
def cad_box(v, m, s, fd):
    v = (20000+v) * -1
    cmd = 'in %d_cad_box bot %d %d 2 1' % (prim, v*3, v)

    faces = ''

    for i in xrange(0, v):
        tokens = fd.readline().split()
        cmd += ' %s %s %s %s %s %s %s %s %s' % tuple(tokens[0:9])
        faces += ' %d %d %d ' % (i*3, i*3+1, i*3+2)

    cmd += faces
    outfile.write(cmd+'\n')
    outfile.write('r %d.r u %d_cad_box\n' % (prim, prim))
    assign_material('%d.r' % prim, m)
    

#
#  Wish we had examples of this to develop with
#
def ring_box(v, m, s, fd):
    print 'ring_box not implemented'

    v = (v * -1) - 10000
    t = float(fd.readline())
    b = float(fd.readline())
    for i in xrange(0, v):
        tup = [float(x) for x in fd.readline().split()]
    

#
# Triangle strip, like a FASTGEN plate-mode thing.
#
def tri_strip(v, m, s, fd):
    v = v * -1

    t = float(fd.readline())

    faces = ''
    thicks = ''
    cmd = 'in %d_tri_strip bot %d %d 3 1' % (prim, v, v-2)
    cmd += ' %s %s %s' % tuple(fd.readline().split())
    cmd += ' %s %s %s' % tuple(fd.readline().split())

    for i in xrange(2, v):
        cmd += ' %s %s %s' % tuple(fd.readline().split())
        faces += ' %d %d %d' % (i-2, i-1, i)
        thicks += ' 0 %g' % t

    outfile.write(cmd + faces + thicks + '\n')
    outfile.write('r %d.r u %d_tri_strip\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
#  Wish we had examples of this to develop with.  Is it always 2 replicas?
#
def replica(v, m, s, fd):
    print 'replica not implemented'
    tup = [0, 0, 0]
    while len(tup) != 1:
        buf = fd.readline()
        if buf == '0':
            break
        else:
            tup = [float(x) for x in fd.readline().split()]

    m = fd.readline()
    s = fd.readline()
    tup = [float(x) for x in fd.readline().split()]

    m = fd.readline()
    s = fd.readline()
    tup = [float(x) for x in fd.readline().split()]
        
#
#  The documentation calls for butt-aligned cylinders here.
#  I've put spheres in.  Should probably change this
#
def multiwire(v, m, s, fd):
    n = int(fd.readline())
    cmd = 'in %d_pipe pipe %d' % (prim, n)

    pts = []
    for i in xrange(0, n):
        pts.append([float(x) for x in fd.readline().split()])

    r = float(fd.readline())

    comb = ''
    for i in xrange(1, n):
        v = [pts[i][s] - pts[i-1][s] for s in [0, 1, 2]]
        name = '%d_pipe%d' % (prim, i)
        comb += ' ' + name
        args = [name] + pts[i-1] + v + [r]
        outfile.write('in %s rcc %g %g %g %g %g %g %g\n' % tuple(args))
                  
        if i < (n-1):
            name = '%d_pipe_s%d' % (prim, i)
            comb += ' ' + name
            args = [name] + pts[i] + [r]
            outfile.write('in %s sph %g %g %g %g \n' % tuple(args))

    outfile.write('g %d_pipe.g %s\n' % (prim, comb))
    outfile.write('r %d.r u %d_pipe.g\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
# This is really just another kind of cylinder
#
def wire(v, m, s, fd):
    p = [float(x) for x in fd.readline().split()]
    p +=[float(x) for x in fd.readline().split()]
    p.append(float(fd.readline()))

    outfile.write('in %d_wire rcc %g %g %g  %g %g %g  %g\n' % tuple(p))
    outfile.write('r %d.r u %d_wire\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
# Axis aligned box
#
def rpp(v, m, s, fd):
    lo = [float(x) for x in fd.readline().split()]
    hi = [float(x) for x in fd.readline().split()]

    cmd = 'in %d_rpp rpp %g %g %g %g %g %g' % (prim, min(lo[0], hi[0]), max(lo[0], hi[0]),
                                               min(lo[1], hi[1]), max(lo[1], hi[1]),
                                               min(lo[2], hi[2]), max(lo[2], hi[2]))
    outfile.write(cmd+'\n')
    outfile.write('r %d.r u %d_rpp\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
# This is a BRL-CAD trc
#
def cone(v, m, s, fd):
    a = [float(x) for x in fd.readline().split()]
    ra = float(fd.readline())
    b = [float(x) for x in fd.readline().split()]
    v = [b[i]-a[i] for i in [0, 1, 2]]

    rb = float(fd.readline())
    p = [prim]
    p += a
    p += v
    p += [ra, rb]

    outfile.write('in %d_cone trc %g %g %g  %g %g %g  %g %g\n' % tuple(p))
    outfile.write('r %d.r u %d_cone\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
#  BRL-CAD rcc
#
def cyl(v, m, s, fd):
    a = [float(x) for x in fd.readline().split()]
    b = [float(x) for x in fd.readline().split()]


    p = [prim]
    p += a
    v = [b[i]-a[i] for i in [0, 1, 2]]
    p += v
    p += [float(fd.readline())]

    outfile.write('in %d_cyl rcc %g %g %g %g %g %g %g\n' % tuple(p))
    outfile.write('r %d.r u %d_cyl\n' % (prim, prim))
    assign_material('%d.r' % prim, m)

#
#  Spec doesn't say if faces can be non-planar.  I hope not.
#
def arb(v, m, s, fd):
    cmd = 'in %d_arb arb8' % prim

    pts = []
    for i in xrange(0, 8):
        pts.append([float(x) for x in fd.readline().split()])


    for i in [pts[6], pts[2], pts[4], pts[5], pts[0], pts[1], pts[3], pts[7]]:
        cmd += ' %s %s %s' % tuple(i)

    outfile.write(cmd+'\n')
    outfile.write('r %d.r u %d_arb\n' % (prim, prim))
    assign_material('%d.r' % prim, m)


short_forms = {
0 : replica,
-1 : multiwire,
1 : wire,
2 : rpp,
3 : cone,
5 : cyl,
8 : arb
}





# set the title
outfile.write('title %s' % fd.readline())

# process each record
str = fd.readline()
while str:
    verts = int(str)
    mater = int(fd.readline())
    surround = int(fd.readline())

    coords = []
    faces = []
    if (verts < -20000):
        cad_box(verts, mater, surround, fd)
    elif (verts < -10000):
        ringbox(verts, mater, surround, fd)
    elif short_forms.has_key(verts):
        short_forms[verts](verts, mater, surround, fd)
    elif verts < 0:
        tri_strip(verts, mater, surround, fd)
    else:
        long_form(verts, mater, surround, fd)

    str = fd.readline()
    prim += 1

logfile.close()




if len(used_materials) > 0:
    cmd = 'g materials.g'
    for i in used_materials:
        cmd += ' ' + i
    outfile.write(cmd+'\n')

if len(used_components) > 0:
    cmd = 'g components.g'
    for i in used_components:
        cmd += ' ' + i
    outfile.write(cmd+'\n')


