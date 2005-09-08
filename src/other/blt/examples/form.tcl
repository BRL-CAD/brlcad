#!../src/bltwish

package require BLT
# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------
if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}
#source scripts/demo.tcl

option add *takeFocus 0

set file1 ../demos/images/chalk.gif
set file2 ../demos/images/tan_paper.gif
image create photo texture1 -file $file1
image create photo texture2 -file $file2
option add *Frame.Tile texture1
option add *Toplevel.Tile texture1
option add *Label.Tile texture1
option add *Scrollbar.tile texture1
#option add *Scrollbar.activeTile texture2
option add *Button.tile texture1
#option add *Button.activeTile texture2
option add *HighlightThickness 0
option add *Entry.highlightThickness 2

#
# Initialization of global variables and Tk resource database
#
#
# Resources available
#
# Tk.normalBgColor: 
# Tk.normalFgColor: 
# Tk.focusHighlightColor: 
# Tk.statusFont: 
# Tk.titleFont: 
# Tk.headingFont: 
# Tk.subheadingFont:
# Tk.entryFont:
# Tk.textFont:
#

#debug 50
bitmap define attlogo { { 60 30 } {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x7e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xf8, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x83, 0xf9, 0x87, 0x7f, 0x00,
    0x00, 0x00, 0x00, 0x80, 0xf9, 0x87, 0x7f, 0x00, 0x40, 0x00, 0xf0, 0xc7,
    0xc3, 0x38, 0x0c, 0x00, 0xc0, 0xff, 0xff, 0xc7, 0xc3, 0x7c, 0x0c, 0x00,
    0x00, 0x00, 0x00, 0x40, 0xc2, 0x6c, 0x0c, 0x00, 0x40, 0x00, 0xf8, 0x67,
    0xc6, 0x9c, 0x0d, 0x00, 0xc0, 0xff, 0xff, 0xe7, 0xc7, 0xf8, 0x0d, 0x00,
    0x00, 0x00, 0x00, 0xe0, 0xc7, 0xec, 0x0c, 0x00, 0x80, 0x01, 0xfe, 0x33,
    0xcc, 0xfc, 0x0d, 0x00, 0x00, 0xff, 0xff, 0x33, 0xcc, 0xb8, 0x0d, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
}

bitmap define globe_00 { { 32 32 } {
    00 40 02 00 00 1c 3c 00 00 01 fe 00 80 80 fe 03 60 00 ff 07 10 c0 f1 0f
    00 80 c0 1f 00 c0 07 3f 00 c0 ff 3f 00 f0 ff 4f 02 f0 ff 5d 00 f0 ff 1b
    00 f0 ff 8f 02 f0 ff 0f 06 e0 fc 0f 0e 00 f8 0f 0f 00 f8 07 3f 00 f8 03
    7e 00 f0 03 7e 00 f0 03 3e 00 f0 0b 3c 00 f0 09 3c 00 f0 01 18 00 f0 00
    18 00 70 00 10 00 00 00 10 00 00 00 20 00 00 00 40 00 00 00 00 00 00 00
    00 00 00 00 00 00 1f 00 }
}

bitmap define globe_01 { { 32 32 } {
    00 c0 00 00 00 34 38 00 00 02 e8 00 80 01 fa 03 e0 00 fc 07 30 00 e6 0f
    10 00 86 1f 08 00 3e 3c 04 00 ff 3f 04 80 ff 5f 02 80 ff 3f 00 80 ff 2f
    00 80 ff 3f 0c 00 ff 3f 1c 00 ee 3f 3c 00 c0 3f 7e 00 c0 1f fe 01 80 1f
    fc 03 80 1f fc 01 80 1f fc 01 80 2f f8 01 80 0f f0 00 80 17 f0 00 80 03
    f0 00 80 03 60 00 00 00 60 00 00 00 40 00 00 00 80 00 00 00 00 00 00 00
    00 00 00 00 00 00 1e 00 }
}

bitmap define globe_02 { { 32 32 } {
    00 c0 01 00 00 60 30 00 00 04 f0 00 80 07 e0 03 e0 01 f0 07 f0 00 38 0f
    30 00 10 1e 18 00 f0 30 04 00 f8 3f 10 00 f8 7f 12 00 fc 7f 02 00 fc 7f
    04 00 fc 7f 74 00 f8 7f f0 00 70 7f f8 01 00 7e f8 03 00 7e f8 0f 00 7c
    f8 1f 00 3c f0 1f 00 3c f0 0f 00 3e e0 0f 00 5e c0 07 00 1c c0 03 00 0e
    c0 03 00 04 80 01 00 00 80 01 00 00 80 01 00 00 00 01 00 00 00 00 00 00
    00 00 00 00 00 10 1c 00 }
}

bitmap define globe_03 { { 32 32 } {
    00 c0 01 00 00 dc 20 00 00 09 c0 00 80 1f a0 03 e0 07 c0 07 f0 01 c0 0c
    f8 00 40 18 78 00 c0 23 08 00 c0 3f 04 00 e0 7f 54 00 e0 7f 0c 00 c0 7f
    10 00 c0 ff d0 01 c0 ff c0 03 80 fb e0 0f 00 f0 e0 1f 00 f0 e0 ff 00 f0
    e0 ff 00 70 c0 ff 00 70 c0 7f 00 70 00 7f 00 70 00 3f 00 30 00 1f 00 38
    00 1f 00 18 00 0e 00 00 00 06 00 00 00 02 00 00 00 04 00 00 00 00 00 00
    00 00 00 00 00 20 18 00 }
}

bitmap define globe_04 { { 32 32 } {
    00 c0 03 00 00 7c 03 00 00 13 00 00 80 7f c0 03 c0 1f 00 07 e0 0f 00 0d
    f0 03 00 10 f0 01 00 0e 38 01 00 3e 10 00 00 7f 50 00 00 7f 30 00 00 7f
    40 00 00 ff 00 1e 00 fe 00 3f 00 ec 00 7f 00 c0 00 ff 00 c0 00 ff 07 c0
    00 ff 0f c0 00 fe 07 c0 00 fe 07 c0 00 f8 03 40 00 f8 01 60 00 f8 00 20
    00 f8 00 20 00 38 00 00 00 18 00 00 00 18 00 00 00 18 00 00 00 00 00 00
    00 00 00 00 00 40 10 00 }
}

bitmap define globe_05 { { 32 32 } {
    00 c0 03 00 00 bc 06 00 00 cf 00 00 80 ff 01 02 c0 7f 00 06 c0 3f 00 0e
    e0 1f 00 14 e0 0f 00 18 e0 00 00 38 60 00 00 78 40 08 00 78 c0 01 00 78
    00 02 00 f8 00 f0 00 f0 00 f0 01 b0 00 f8 07 80 00 f8 0f 80 00 f8 3f 00
    00 f8 7f 00 00 f0 3f 80 00 f0 3f 80 00 c0 1f 00 00 c0 0f 00 00 c0 07 40
    00 c0 07 00 00 c0 01 00 00 e0 00 00 00 60 00 00 00 40 00 00 00 00 00 00
    00 00 00 00 00 80 10 00 }
}

bitmap define globe_06 { { 32 32 } {
    00 80 07 00 00 7c 0d 00 00 9f 03 00 00 ff 07 02 00 ff 03 04 80 ff 00 08
    c0 7f 00 00 80 3f 00 30 80 07 00 20 00 03 00 60 00 03 00 60 00 0e 00 60
    00 10 00 e0 00 80 07 c0 00 80 0f c0 00 80 3f 00 00 c0 7f 00 00 c0 ff 01
    00 c0 ff 03 00 80 ff 01 00 80 ff 01 00 00 fe 00 00 00 7e 00 00 00 3e 00
    00 00 1f 00 00 00 0f 00 00 00 03 00 00 00 03 00 00 00 01 00 00 00 00 00
    00 00 00 00 00 00 01 00 }
}

bitmap define globe_07 { { 32 32 } {
    00 80 07 00 00 fc 1a 00 00 7d 02 00 00 fe 1f 00 00 fe 0f 00 00 fe 07 00
    00 ff 03 00 00 fe 01 20 00 1c 01 00 00 1c 00 40 00 18 00 40 00 70 00 00
    00 80 00 80 00 00 39 80 00 00 7c 00 00 00 fc 01 00 00 fe 03 00 00 fe 0f
    00 00 fc 0f 00 00 fc 0f 00 00 f8 07 00 00 f0 07 00 00 f0 03 00 00 f0 01
    00 00 f8 00 00 00 38 00 00 00 18 00 00 00 0c 00 00 00 04 00 00 00 00 00
    00 00 00 00 00 00 02 00 }
}

bitmap define globe_08 { { 32 32 } {
    00 00 07 00 00 fc 25 00 00 f8 19 00 00 f8 7f 00 00 f8 3f 00 00 f8 1f 00
    00 f8 1f 00 00 f8 0f 00 00 f0 08 00 00 f0 00 00 00 c0 04 00 00 80 03 00
    00 00 0c 00 00 00 c8 01 00 00 e0 03 00 00 e0 0f 00 00 e0 0f 00 00 f0 3f
    00 00 e0 3f 00 00 e0 3f 00 00 c0 1f 00 00 80 1f 00 00 80 0f 00 00 c0 07
    00 00 c0 03 00 00 c0 01 00 00 60 00 00 00 30 00 00 00 10 00 00 00 00 00
    00 00 00 00 00 00 04 00 }
}

bitmap define globe_09 { { 32 32 } {
    00 00 03 00 00 fc 27 00 00 f0 13 00 00 e0 ff 00 00 e0 ff 01 00 e0 7f 00
    00 e0 7f 00 00 c0 7f 00 00 80 47 00 00 80 07 00 00 00 26 00 00 00 1c 00
    00 00 60 00 00 00 40 0e 00 00 00 1f 00 00 00 3f 00 00 00 3f 00 00 00 7f
    00 00 00 7f 00 00 00 7f 00 00 00 7e 00 00 00 3c 00 00 00 3e 00 00 00 1e
    00 00 00 0f 00 00 00 07 00 00 80 01 00 00 80 00 00 00 40 00 00 00 00 00
    00 00 00 00 00 00 08 00 }
}

bitmap define globe_10 { { 32 32 } {
    00 00 06 00 00 f4 2f 00 00 c8 4f 00 00 80 ff 01 00 80 ff 01 00 80 ff 01
    00 00 ff 01 00 00 fe 01 00 00 3c 00 00 00 3c 00 00 00 30 04 00 00 e0 00
    00 00 00 01 00 00 00 3a 00 00 00 38 00 00 00 78 00 00 00 f8 00 00 00 fc
    00 00 00 f8 00 00 00 f8 00 00 00 f8 00 00 00 70 00 00 00 70 00 00 00 38
    00 00 00 18 00 00 00 0c 00 00 00 06 00 00 00 03 00 00 00 00 00 00 00 00
    00 00 00 00 00 00 10 00 }
}

bitmap define globe_11 { { 32 32 } {
    00 00 06 00 00 ec 1f 00 00 91 9f 00 00 00 fe 03 00 00 fc 07 00 00 fc 07
    00 00 fc 07 00 00 f0 07 00 00 f0 01 00 00 e0 00 00 00 80 05 00 00 00 07
    00 00 00 08 00 00 00 60 00 00 00 e0 00 00 00 e0 00 00 00 e0 00 00 00 e0
    01 00 00 e0 00 00 00 e0 00 00 00 e0 00 00 00 40 00 00 00 60 00 00 00 60
    00 00 00 30 00 00 00 10 40 00 00 08 40 00 00 04 00 00 00 00 00 00 00 00
    00 00 00 00 00 00 10 00 }
}

bitmap define globe_12 { { 32 32 } {
    00 00 04 00 00 dc 3f 00 00 42 7e 00 00 00 f8 03 20 00 f0 07 10 00 f0 0f
    00 00 e0 0f 00 00 c0 0f 00 00 00 07 00 00 00 06 00 00 00 14 00 00 00 18
    00 00 00 20 00 00 00 80 00 00 00 80 00 00 00 80 00 00 00 80 01 00 00 80
    00 00 00 80 00 00 00 80 02 00 00 80 02 00 00 00 04 00 00 40 04 00 00 40
    08 00 00 20 08 00 00 00 00 00 00 10 00 00 00 08 00 00 00 00 00 00 00 00
    00 00 00 00 00 00 00 00 }
}

bitmap define globe_13 { { 32 32 } {
    00 00 04 00 00 bc 3f 00 00 01 79 00 80 00 e0 03 60 00 c0 07 10 00 80 0f
    00 00 80 1f 08 00 00 1e 00 00 00 1c 00 00 00 58 00 00 00 10 00 00 00 20
    00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 06 00 00 00
    04 00 00 00 00 00 00 00 02 00 00 00 0e 00 00 00 0c 00 00 00 1c 00 00 00
    18 00 00 00 30 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 10 00 00 }
}

bitmap define globe_14 { { 32 32 } {
    00 00 00 00 00 fc 3f 00 00 03 e6 00 80 01 c0 03 60 00 00 07 30 00 00 0f
    00 00 00 1e 00 00 00 38 04 00 00 30 00 00 00 30 02 00 00 00 00 00 00 40
    00 00 00 80 00 00 00 00 02 00 00 00 01 00 00 00 01 00 00 00 18 00 00 00
    00 00 00 00 00 00 00 00 14 00 00 00 3c 00 00 00 3c 00 00 00 7c 00 00 00
    78 00 00 00 60 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 30 00 00 }
}

bitmap define globe_15 { { 32 32 } {
    00 00 00 00 00 fc 3d 00 00 27 c8 00 80 13 00 03 e0 01 00 06 70 00 00 0c
    10 00 00 18 18 00 00 30 0c 00 00 20 0c 00 00 40 02 00 00 00 02 00 00 00
    00 00 00 00 00 00 00 00 01 00 00 00 03 00 00 00 13 00 00 00 64 00 00 00
    c0 00 00 00 00 00 00 00 30 00 00 00 f8 00 00 00 f8 01 00 00 f8 03 00 00
    f0 03 00 00 80 03 00 00 00 80 00 00 00 40 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 70 00 00
  }
}

bitmap define globe_16 { { 32 32 } {
    00 00 00 00 00 fc 3b 00 00 9f a0 00 80 4f 00 02 e0 0f 00 04 f0 01 00 08
    70 00 00 10 38 00 00 20 3c 00 00 00 1c 00 00 00 06 00 00 00 04 00 00 00
    04 00 00 00 00 00 00 00 20 00 00 00 0a 00 00 00 0a 00 00 00 00 03 00 00
    28 06 00 00 00 00 00 00 c0 02 00 00 e0 07 00 00 f0 0f 00 00 e0 1f 00 00
    e0 1f 00 00 00 0c 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 00 00 }
}

bitmap define globe_17 { { 32 32 } {
    00 00 00 00 00 fc 37 00 00 3f 42 00 80 3f 01 02 e0 1f 00 00 f0 07 00 00
    f0 11 00 00 f8 04 00 00 fc 00 00 00 7c 00 00 00 1a 00 00 00 9a 00 00 00
    18 00 00 00 00 00 00 00 00 00 00 00 20 00 00 00 28 00 00 00 08 18 00 00
    00 30 00 00 00 10 00 00 00 17 00 00 00 3f 00 00 c0 7f 00 00 80 7f 00 00
    80 7f 00 00 00 70 00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 01 00
  }
}

bitmap define globe_18 { { 32 32 } {
    00 00 00 00 00 fc 2f 00 00 ff 84 00 80 ff 04 00 e0 7f 00 00 f0 9f 00 00
    f0 97 00 00 f8 27 00 00 fc 07 00 00 fc 03 00 00 6c 00 00 00 64 00 00 00
    60 04 00 00 40 00 00 00 20 00 00 00 20 01 00 00 a0 01 00 00 00 c0 05 00
    00 88 00 00 00 00 00 00 00 38 01 00 00 fc 01 00 00 fe 03 00 00 fe 03 00
    00 fc 03 00 00 80 03 00 00 00 00 00 00 00 20 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 03 00
  }
}

bitmap define globe_19 { { 32 32 } {
    00 40 00 00 00 fc 3f 00 00 ff 13 00 80 ff 13 00 e0 ff 03 00 f0 ff 00 00
    f0 9f 00 00 f8 3f 00 00 fc 3f 00 00 f8 1f 00 00 ba 07 00 00 98 23 00 00
    08 03 00 00 08 00 00 00 00 00 00 00 80 09 00 00 00 0d 01 00 00 21 0e 00
    00 00 1c 00 00 00 00 00 00 c0 09 00 00 e0 0f 00 00 f0 1f 00 00 f0 1f 00
    00 f0 1f 00 00 00 0e 00 00 00 00 00 00 00 80 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 07 00
  }
}

bitmap define globe_20 { { 32 32 } {
    00 00 00 00 00 fc 3f 00 00 ff 07 00 80 ff 0f 00 e0 ff 0f 00 f0 ff 13 00
    f0 ff 10 00 f8 ff 00 00 fc ff 01 00 f4 ff 00 00 e6 1e 00 00 62 1c 01 00
    20 18 00 00 20 10 00 00 01 80 00 00 01 cc 00 00 01 68 08 00 00 00 60 00
    00 00 c0 00 00 00 00 00 00 00 5c 00 00 00 7e 00 00 80 ff 00 00 80 ff 00
    00 80 ff 00 00 00 70 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 0f 00
  }
}

bitmap define globe_21 { { 32 32 } {
    00 80 00 00 00 fc 3f 00 00 ff 1f 00 80 ff bf 00 e0 ff 3f 00 f0 ff 1f 00
    f8 ff 17 00 f8 ff 27 00 ec ff 0f 00 8c ff 07 00 9e f7 00 00 0e e3 00 00
    06 c1 00 00 06 81 10 00 03 40 04 00 03 20 06 00 03 40 06 00 01 80 00 03
    01 00 00 02 02 00 00 00 02 00 e0 02 02 00 f0 03 00 00 fc 03 00 00 fc 03
    00 00 fc 03 00 00 c0 01 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_22 { { 32 32 } {
    00 00 01 00 00 fc 3f 00 00 ff 3f 00 80 ff 7f 00 e0 ff ff 00 f0 ff 7f 00
    f0 ff 1f 00 e0 ff 3f 00 fc ff 3f 00 34 fe 3f 00 76 bc 07 00 36 1c 07 00
    0e 08 0e 00 1e 08 80 00 0f 00 02 00 0f 00 20 00 07 00 36 00 07 00 04 08
    07 00 00 18 06 00 00 00 16 00 00 0b 16 00 80 0f 04 00 e0 0f 04 00 e0 0f
    08 00 e0 0f 00 00 00 06 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_23 { { 32 32 } {
    00 00 00 00 00 fc 3f 00 00 ff 7f 00 80 ff ff 01 e0 ff ff 01 e0 ff ff 01
    e8 ff ff 00 c0 ff ff 00 fc fe ff 01 dc f2 ff 01 de e3 3d 00 de e1 38 02
    7e 40 70 00 fe 40 00 04 7f 00 00 00 3e 00 30 01 3e 00 a0 01 1e 00 20 20
    1e 00 00 20 1c 00 00 00 9c 00 00 3c 1c 00 00 3e 1c 00 00 3f 18 00 80 3f
    10 00 00 1f 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_24 { { 32 32 } {
    00 00 02 00 00 fc 3f 00 00 fe ff 00 80 ff ff 01 c0 ff ff 03 80 ff ff 03
    e0 ff ff 03 18 ff ff 03 fc ff ff 07 7c 87 ff 07 fe 1f ef 01 fe 0e c6 01
    fe 01 82 03 fe 03 02 00 ff 03 00 08 fc 01 80 09 fc 00 00 0d fc 00 00 00
    f8 00 00 80 f8 00 00 00 f8 00 00 20 78 02 00 70 70 02 00 7c 70 00 00 3c
    60 00 00 3c 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_25 { { 32 32 } {
    00 00 00 00 00 f0 3f 00 00 fc ff 00 80 ff ff 03 80 ff ff 07 a0 ff ff 07
    10 ff ff 07 30 f8 ff 0f f8 df ff 1f fc 3b fc 1f fc fb 78 07 fe 77 30 0e
    fe 1f 30 0c fe 3f 00 48 fe 1f 00 00 f0 0f 00 24 f0 07 00 a0 f0 07 00 08
    e0 07 00 00 e0 07 00 00 e0 27 00 c0 e0 13 00 40 c0 13 00 70 c0 03 00 70
    80 01 00 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_26 { { 32 32 } {
    00 40 00 00 00 e0 3f 00 00 e8 ff 00 00 fc ff 03 00 ff ff 07 c0 fe ff 0f
    40 f0 ff 1f e0 e0 ff 1f f0 ff fe 3f f8 df e1 3f f8 df c7 1b fc bf 83 19
    fc ff 80 30 fc ff 01 20 f8 ff 00 00 c0 ff 00 00 c0 7f 00 e0 80 3f 00 20
    80 3f 00 00 80 3f 00 00 80 3f 01 80 80 9f 00 00 00 9f 00 40 00 0f 00 60
    00 0e 00 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 f0 1f 00 }
}

bitmap define globe_27 { { 32 32 } {
    00 80 00 00 00 c4 3f 00 00 f0 ff 00 00 fe ff 03 00 fe ff 07 00 eb ff 0f
    80 c9 ff 1f 80 07 ff 3f c0 ff f7 3f e0 ff 0e 7f f0 ff 3e 6e f0 ff 1d 64
    f0 ff 07 44 f0 ff 0f 00 60 ff 0f 00 00 fe 07 40 00 fe 03 00 01 fc 01 00
    01 fc 01 00 00 fc 01 00 00 fc 09 00 02 fc 08 00 00 f8 04 00 00 78 00 40
    00 70 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 e0 1f 00 }
}

bitmap define globe_28 { { 32 32 } {
    00 00 00 00 00 88 3f 00 00 40 ff 00 00 e8 ff 03 00 f8 ff 07 00 8c ff 0f
    00 06 fe 1f 00 1e f8 3f 00 ff bf 3f 80 ff 77 7c 80 ff ff 79 c0 ff ef 10
    c0 ff 3f 90 c0 ff 7f 00 81 fb 7f 00 01 f0 3f 00 01 f0 1f 00 03 e0 1f 00
    07 e0 0f 00 02 c0 1f 00 02 e0 5f 00 06 e0 47 00 04 c0 27 00 04 c0 03 00
    00 80 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 c0 1f 00 }
}

bitmap define globe_29 { { 32 32 } {
    00 40 01 00 00 0c 3f 00 00 80 fd 00 00 a0 ff 03 20 e0 ff 07 00 30 fd 0f
    00 10 f4 1f 00 f8 c0 3f 00 f8 ff 3f 00 fc bf 73 00 fe ff 67 00 fe 7f 47
    00 fe ff 41 00 fe ff 03 01 dc ff 03 03 00 ff 01 07 80 ff 00 0f 00 ff 00
    1f 00 7e 00 0e 00 fe 00 0e 00 ff 02 0e 00 3f 01 0c 00 3e 01 0c 00 1e 00
    08 00 1c 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 00 00 00 80 1f 00 }
}

# ----------------------------------------------------------------
#
# SetOption --  
#	
#	Sets the option array associated with the resource. It
# 	first check to see if the resource exists in the option
#	data base, otherwise it uses the default value given.
#	
#
# Arguments
#	name		-- Name of the resource. Used as index into
#			   the option array.
#	value		-- default value given.
# Globals
#	pq_dict  	-- Associative array where the option resources
#			   are stored.
#
# ----------------------------------------------------------------

proc SetOption { name value } {
    global pq_dict
    set widgetOption [option get . $name Tk]
    if { $widgetOption != "" } {
	set value $widgetOption
    }
    set pq_dict($name) $value
}

set pq_dict(textIndex) {}

set pq_dict(entryNames) {
    last first middle 
    area exch ext 
    com org tl
    room oldloc loc 
    street 
    city state zip 
    ema
}

set pq_dict(numEntries) [llength $pq_dict(entryNames)]
set pq_dict(index) 0
set pq_dict(defaults) {}

set cnt 0
foreach name $pq_dict(entryNames) {
    if { $cnt > 0 } {
	set pq_dict(format) $pq_dict(format):%$name
    } else {
	set pq_dict(format) %$name
    }
    incr cnt
}

set visual [winfo screenvisual .]
if { ($visual == "staticgray") || ($visual == "grayscale") } {
    option add *Entry.background white
    option add *Text.background white
    set pq_dict(visual) mono
} else {
    set depth [winfo screendepth .]
    if { $depth < 8 } {
	SetOption normalBgColor grey
	SetOption normalFgColor black
	SetOption focusHighlightColor white
    } else {
#fff4de
	SetOption normalBgColor grey90
	SetOption normalFgColor #da5f5f
	SetOption normalFgColor navyblue
	SetOption focusHighlightColor #fffdf8 
    }
    option add *Entry.background $pq_dict(normalBgColor) widgetDefault
    option add *Text.background $pq_dict(normalBgColor) widgetDefault
    option add *Label.foreground $pq_dict(normalFgColor) widgetDefault
    option add *Button.foreground $pq_dict(normalFgColor) widgetDefault
    set pq_dict(visual) color
}    

SetOption statusFont 	 -*-Helvetica-Medium-R-*-*-14-*-*-*-*-*-*-*
SetOption titleFont	 -*-Helvetica-Bold-R-*-*-14-*-*-*-*-*-*-*
SetOption headingFont 	 -*-Helvetica-Medium-R-*-*-14-*-*-*-*-*-*-*
SetOption subheadingFont -*-Helvetica-Medium-R-*-*-12-*-*-*-*-*-*-* 
SetOption entryFont 	 -*-Courier-Medium-R-*-*-12-*-*-*-*-*-*-*
SetOption textFont 	 -*-Courier-Bold-R-*-*-12-*-*-*-*-*-*-*
#SetOption entryFont 	 fixed
#SetOption textFont 	 fixed

option add *Label.borderWidth 	0		    widgetDefault
option add *Entry.relief 	sunken              widgetDefault
option add *Entry.width 	11		    widgetDefault
option add *Entry.borderWidth 	2		    widgetDefault
option add *Entry.font 		$pq_dict(entryFont) widgetDefault
option add *Text.font		$pq_dict(textFont)  widgetDefault
option add *Text.width 		35		    widgetDefault
option add *Text.height 	10 		    widgetDefault
option add *Scrollbar.relief 	flat		    widgetDefault
option add *Scrollbar.minSlider	10		    widgetDefault
option add *Button.padY 6
option add *Text.relief 	sunken		    widgetDefault
option add *Text.borderWidth 	2 		    widgetDefault

foreach name $pq_dict(entryNames) {
    option add *${name}_label.font $pq_dict(subheadingFont) widgetDefault
}

option add *Label.Font          $pq_dict(subheadingFont)
option add *status_label.font   $pq_dict(statusFont)  widgetDefault
option add *name_label.font   	$pq_dict(headingFont) widgetDefault
option add *tel_label.font 	$pq_dict(headingFont) widgetDefault
option add *office_label.font 	$pq_dict(headingFont) widgetDefault
option add *addr_label.font 	$pq_dict(headingFont) widgetDefault
option add *loc_title.font 	$pq_dict(headingFont) widgetDefault
option add *org_title.font 	$pq_dict(headingFont) widgetDefault

option add *overall_label.text	"Customer Database"
option add *name_label.text	"Name"
option add *tel_label.text 	"Telephone"
option add *addr_label.text	"Address"
option add *last_label.text	"last"
option add *first_label.text	"first"
option add *middle_label.text	"middle"
option add *com_label.text	"company"
option add *org_label.text      "organization"
option add *tl_label.text	"title"
option add *ext_label.text	"extension"
option add *exch_label.text	"exchange"
option add *area_label.text	"area code"
option add *loc_label.text	"extension"
option add *oldloc_label.text	"exchange"
option add *room_label.text	"area code"
option add *street_label.text	"street address"
option add *ema_label.text	"e-mail"
option add *city_label.text	"city"
option add *state_label.text	"state"
option add *zip_label.text	"zip code"
option add *org_title.text	"Organization"
option add *loc_title.text	"Fax"

option add *clear_button.text	"Clear"
option add *quit_button.text	"Quit"
option add *cancel_button.text	"Cancel"

# --------------------------------------------------------------------------
#
# Procedures to perform post queries
#

# ----------------------------------------------------------------
#
# StopQuery --  
#	
#	Stops any current "pq" request by setting the variable
#	associated with the background subprocesses. 
#
# Arguments
#	None.
#
# Globals
#	postOutput 	-- variable where collected output from 
#			   pq command will be stored
#
# ----------------------------------------------------------------

proc StopQuery {} {
    global postOutput
    set postOutput {}
}


# ----------------------------------------------------------------
#
# PostQuery --  
#	
#	Collects the data from the entry widget fields and 
#	executes a "pq" request.  The "pq" command is executed 
#	in the background and a "wait" is setup to wait for the 
#	command to finish.  This allows the animation routine
#	to operate and exposure events to be handled properly.
#
# Arguments
#	None.
#
# Globals
#	postOutput 	    -- variable where collected output from 
#			       pq command will be stored
#	pq_dict(entryNames) -- list of entry widget names
#	pq_dict(textIndex)  -- starting index of highlighted information
#			       (a line in the text widget)
#
# ----------------------------------------------------------------

proc PostQuery {} {
    global pq_dict 

    .status_label configure -text {}
    set cnt 0
    foreach name $pq_dict(entryNames) {
	set value [.${name}_entry get]
	if { $value != "" } {
	    set value [split $value "|"]
    	    foreach i $value {
	        if { $cnt > 0 } {
		    set query $query/$name=[list $i]
	        } else {
		    set query $name=[list $i]
	        }
	        incr cnt
 	    }
	}
    }
    if { $cnt == 0 } {
	return
    }
    set fmt {%^24pn\t%10org\t%6loc\t%area-%exch-%ext\t%ema}
    global postOutput postError
    set postOutput {}
    set postError {}
    bgexec postStatus -error postError -output postOutput \
	pq -o $fmt $query &
    Animate on
    tkwait variable postStatus
    if { $postOutput != "" } {
	.text configure -state normal
	.text delete 0.0 end
	.text insert 0.0 $postOutput
	.text configure -state disabled
	.status_label configure -text "Post query successful"
    } else {
	.status_label configure -text "Post query failed"
    }
    set pq_dict(textIndex) {}
    Animate off
    if { $postError != "" }  {
        tkerror $postError
    }
}


# ----------------------------------------------------------------
#
# ClearFields --  
#	
#	Clears the all the entry fields.
#
# Arguments
#	None.
#
# Globals
#	pq_dict(entryNames) -- list of entry widget names
#	pq_dict(textIndex)  -- starting index of highlighted information
#			       (a line in the text widget)
#
# ----------------------------------------------------------------

proc ClearFields {} {
    global pq_dict 

    busy hold . ; update
    foreach name $pq_dict(entryNames) {
	.${name}_entry delete 0 end
    }
    set pq_dict(textIndex) {}
    .status_label configure -text "Cleared query fields"
    busy release .
}


# ----------------------------------------------------------------
#
# FillFields --  
#	
#	Makes a post query based upon the highlighted line in
#	the text widget to fill in all post entry fields.
#
# Arguments
#	x		    x screen coordinate
#	y 		    y screen coordinate
#
# Globals
#	postOutput 	    variable where collected output from pq 
#			    command will be stored
#	pq_dict(format)     standard query format to collect data for 
#			    all entry fields
#	pq_dict(entryNames) list of entry widget names
#
# ----------------------------------------------------------------

proc FillFields { x y } {
    global pq_dict 

    set info [.text get [list @$x,$y linestart] [list @$x,$y lineend]]
    set info [split $info \t]
    if { [llength $info] == 0 } {
	return
    }
    set name [string trim [lindex $info 0]]
    set name [split $name ,]
    set last [lindex $name 0]
    set name [split [lindex $name 1]]
    set first [lindex $name 0]
    set middle [lindex $name 1]
    set org [string trim [lindex $info 1]]
    set loc [string trim [lindex $info 2]]
    set tel [string trim [lindex $info 3]]
    set query last=$last/first=$first/middle=$middle/org=$org/loc=$loc/tel=[list $tel]
    global postOutput
    set postOutput {}
    bgexec postStatus -output postOutput \
	pq -o $pq_dict(format) $query &
    Animate on
    tkwait variable postStatus

    if { $postOutput == "" } {
	# Try again with out the telephone number
	set query last=$last/first=$first/middle=$middle/org=$org/loc=$loc
	set postStatus {}
	bgexec postStatus -output postOutput \
		pq -o $pq_dict(format) $query &
	tkwait variable postStatus
    }	
    Animate off
    if { $postOutput == "" } {
	.status_label configure -text "Post query failed"
    } else {
        .status_label configure -text "Post database fields found"
        set postOutput [split $postOutput : ]
	set cnt 0
	foreach name $pq_dict(entryNames) {
	    .${name}_entry delete 0 end
	    .${name}_entry insert 0 [lindex $postOutput $cnt]
	    incr cnt
	}
    }
}


# ----------------------------------------------------------------
#
# HighlightText --  
#	
#	Highlight the text under the current line (as based upon
#	the given screen coordinates.  Only highlight the line if
#	pointer has been moved to the another line.
#
# Arguments
#	x		    x screen coordinate
#	y 		    y screen coordinate
#
# Globals
#	pq_dict(visual)     either "mono" or "color"; indicates if
#			    color screen features can be used
#	pq_dict(textIndex)  starting index of highlighted information
#	pq_dict(normalFgColor)    color to use for highlighted region
#
# ----------------------------------------------------------------

proc HighlightText { x y } {
    global pq_dict

    set newIndex [.text index [list @$x,$y linestart]]
    if { $newIndex != $pq_dict(textIndex) } {
	catch { .text tag delete highlight }
	.text tag add highlight $newIndex [list $newIndex lineend]
	if { $pq_dict(visual) == "color" } {
	    .text tag configure highlight \
		-foreground $pq_dict(normalFgColor) -underline on
	} else {
	    .text tag configure highlight -underline on 
	}
	set pq_dict(textIndex) $newIndex
    }
}


# ----------------------------------------------------------------
#
# ChangeFocus --  
#	
#	Change the keyboard focus to the next/last entry widget.
#
# Arguments
#	direction	    either "next" or "last"; indicates in 
#			    which direction to change focus
#
# Globals
#	pq_dict(entryNames) list of entry widget names
#	pq_dict(index)      current index in list of entry widget 
#			    names of the keyboard focus. An index
#			    of -1 indicates there is no focus.
#	pq_dict(numEntries) number of names in entry widget list
#
# ----------------------------------------------------------------

proc ChangeFocus direction {
    global pq_dict 

    case $direction {
	next {
	    incr pq_dict(index)
	    if { $pq_dict(index) == $pq_dict(numEntries) } {
		set pq_dict(index) 0
	    }
	}
	last {
	    set pq_dict(index) [expr $pq_dict(index)-1]
	    if { $pq_dict(index) < 0 } {
		set pq_dict(index) [expr $pq_dict(numEntries)-1]
	    }
	}
    }
    focus .[lindex $pq_dict(entryNames) $pq_dict(index)]_entry 
    update idletasks
    update
}


# ----------------------------------------------------------------
#
# ColorFocus --  
#	
#	Change background color of entry widget with active 
#	keyboard focus
#
# Arguments
#	w		    name of entry widget to change
#	bool		    either "on" or "off"; indicates if
#			    the focus highlight should turned on
#			    or off.
#
# Globals
#	pq_dict(entryNames) list of entry widget names
#	pq_dict(index)      current index in list of entry widget 
#			    names of the keyboard focus. An index
#			    of -1 indicates there is no focus.
#	pq_dict(visual)     either "mono" or "color"; indicates if
#			    color screen features can be used
#
# ----------------------------------------------------------------

proc ColorFocus { w bool } {
    global pq_dict 

    regexp {\.([a-z]+)_entry} $w dummy name
    if { $pq_dict(visual) == "color" && [info commands $w] == $w } {
	if { $bool == "on" } {
	    set pq_dict(index) [lsearch $pq_dict(entryNames) $name]
	    $w configure -background $pq_dict(focusHighlightColor) 
        } else {	
	    $w configure -background $pq_dict(normalBgColor) 
        }
    }
}

# ----------------------------------------------------------------
#
# Animate --  
#	
#	Activates/deactivates an animated bitmap and busy window.
#	A cancel button is mapped and raised so that it is unaffected
#	by the busy window.
#
# Arguments
#	option		    either "on", "off", or "continue"; 
#			    indicates whether animation should
#			    be started, stoped or continued.
#
# Globals
#	pq_dict(entryNames) list of entry widget names
#	pq_dict(index)      current index in list of entry widget 
#			    names of the keyboard focus. An index
#			    of -1 indicates there is no focus.
#	pq_dict(visual)     either "mono" or "color"; indicates if
#			    color screen features can be used
#
# ----------------------------------------------------------------

set pq_dict(curBitmap) 0
set pq_dict(lastBitmap) 0

proc Animate option {
    global pq_dict

    case $option {
	on {
	    busy hold . 
	    .status_label configure -text "Searching..."
	    global topLevel
	    table $topLevel .cancel_button 18,8 -anchor e -reqwidth .70i
	    winop raise .cancel_button
	    .quit_button configure -state disabled
	    .clear_button configure -state disabled
	    winop raise .cancel_button
	    set pq_dict(lastFocus) [focus]
            focus -force .
	    set pq_dict(curBitmap) $pq_dict(lastBitmap) 
	    update
	} 
	off {
	    table forget .cancel_button
	    .quit_button configure -state normal
	    .clear_button configure -state normal
	    .trademark configure -bitmap attlogo
	    set pq_dict(lastBitmap) $pq_dict(curBitmap)
	    set pq_dict(curBitmap) -1 
	    focus $pq_dict(lastFocus)
	    busy release .
	}
    }
    #
    # Continue with next bitmap
    #
    if { $pq_dict(curBitmap) >= 0 } {
	set bmap [format globe_%0.2d $pq_dict(curBitmap)] 
	.trademark configure -bitmap $bmap
	incr pq_dict(curBitmap)
	if { $pq_dict(curBitmap) >= 29 } {
	    set pq_dict(curBitmap) 0
	}
	after 100 Animate continue
    }
}

	
# --------------------------------------------------------------------------
#
# main body of program
#
frame .frame 
set topLevel .frame

label .overall_label -font  -*-Helvetica-Bold-R-*-*-18-*-*-*-*-*-*-*
label .name_label -font $pq_dict(titleFont)
label .tel_label  -font $pq_dict(titleFont)
label .addr_label -font $pq_dict(titleFont)
label .org_title -font $pq_dict(titleFont)
label .loc_title -font $pq_dict(titleFont)

foreach name $pq_dict(entryNames) {
    label .${name}_label 
    entry .${name}_entry
}
if [info exists env(POST_DEFAULTS)] {
    set pq_dict(defaults) [split $env(POST_DEFAULTS) ":"]
}
foreach i $pq_dict(defaults) {
    set i [split $i "="]
    if { [llength $i] == 2 } {
	set name [lindex $i 0]
	if { [lsearch $pq_dict(entryNames) $name] >= 0 } {
	    .${name}_entry insert 0 [lindex $i 1]
	}
    }
}
label .orders_title -text "Current Orders" \
	-font  -*-Helvetica-Bold-R-*-*-16-*-*-*-*-*-*-*

set font -*-Helvetica-Bold-R-*-*-14-*-*-*-*-*-*-*
button .clear_button -command ClearFields -font $font
button .quit_button -command { exit }  -font $font
button .search_button -text "Search" -font $font 

label .status_label
button .cancel_button -command StopQuery 
#-relief raised
label .trademark -bitmap attlogo 
text .text -yscrollcommand { .vscroll set } -state disabled 
scrollbar .vscroll -command { .text yview } 

table $topLevel \
    .overall_label	0,1 -cspan 10 -pady 5 \
    .name_label  	1,2 \
    .last_entry  	2,2 -cspan 2 \
    .first_entry 	2,4 \
    .middle_entry 	2,5 \
    .last_label 	3,2 \
    .first_label 	3,4 \
    .middle_label 	3,5 \
    .tel_label 		1,7 \
    .area_entry 	2,7 \
    .exch_entry 	2,8 \
    .ext_entry 		2,9 \
    .area_label 	3,7 \
    .exch_label  	3,8 \
    .ext_label 		3,9 \
    .org_title		4,2 \
    .com_entry  	5,2 \
    .org_entry 		5,3 \
    .tl_entry 		5,4 \
    .com_label 		6,2 \
    .org_label 		6,3 \
    .tl_label 		6,4 \
    .loc_title		4,7 \
    .room_entry 	5,7 \
    .oldloc_entry 	5,8 \
    .loc_entry 		5,9 \
    .room_label 	6,7 \
    .oldloc_label 	6,8 \
    .loc_label 		6,9 \
    .addr_label 	8,2 \
    .street_entry       9,2 \
    .ema_entry		9,7 -cspan 2  \
    .street_label       10,2 \
    .city_entry 	11,2 -cspan 2 \
    .state_entry 	11,4 \
    .zip_entry 		11,5 \
    .ema_label       	10,7 -cspan 2 \
    .city_label 	12,2 -cspan 2 \
    .state_label 	12,4 \
    .zip_label 		12,5 \
    .orders_title       16,2  -pady { 4 0 } \
    .text 		17,2 -cspan 8 -fill both -padx 2 \
    .vscroll 		17,10 -anchor center -fill both \
    .status_label	18,4 -cspan 6 -reqwidth {0 4i} \
    .search_button      18,3 -reqwidth .9i -anchor center -pady 8\
    .clear_button       18,5 -reqwidth .9i -anchor center \
    .quit_button        18,8 -reqwidth .9i -anchor center

eval table configure $topLevel \
	[info command .*_label] [info commands .*_title] \
	-anchor w -padx 2 -ipadx 2 
eval table configure $topLevel [info command .*_entry] \
	-fill both -padx 2 
eval table configure $topLevel .name_label .tel_label .org_title \
    .com_label .addr_label .street_entry .street_label \
    -cspan 3  
eval table configure $topLevel .last_entry .ema_entry .city_entry \
    .ema_label .city_label -cspan 2 

table configure $topLevel .overall_label -anchor center
table configure $topLevel r16 -pady { 5 5 } -resize both
table configure $topLevel c0 -width .vscroll
table configure $topLevel c0 c10 -resize none 
table configure $topLevel r3 r6 r10 r12 -resize none
table configure $topLevel r17 -height { 40 {} } 
table configure $topLevel r16 r18 -resize none
table configure $topLevel c6 -pad { 5 5 }

if { $topLevel == ".frame" } {
    table . \
	$topLevel 0,0 -fill both
}

bind .text <Button-2> {
    FillFields %x %y
    continue
}

bind .text <Motion> {
    HighlightText %x %y
    continue
}

bind .text <Enter> {
    set pq_dict(textIndex) {}
    HighlightText %x %y
    set info [.text get [list 0.0 linestart] [list 0.0 lineend]]
    if { $info != "" } {
        .status_label configure -text "Query individual with button-2"
    }
    continue
}

bind .text <Leave> {
    if { [busy isbusy .] != "." } {	
	.text tag delete highlight
	.status_label configure -text ""
    }
    continue
}


bind EntryFocus <Tab> {
    ChangeFocus next
    break
}

bind EntryFocus <Shift-Tab> {
    ChangeFocus last
    break
}

if { $pq_dict(visual) == "color" } {
    bind EntryFocus <FocusIn> { 
	ColorFocus %W on 
    }
    bind EntryFocus <FocusOut> { 
	ColorFocus %W off 
    }
}

bind Entry <Return> PostQuery

foreach name $pq_dict(entryNames) {
    set w .${name}_entry
    bindtags $w [list EntryFocus $w Entry all]
}

focus .last_entry

