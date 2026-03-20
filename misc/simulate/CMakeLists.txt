# misc/simulate/CMakeLists.txt
#
# Scripts and data files for the M35 truck on terrain physics simulation
# scenario.  These are standalone scripts that exercise the BRL-CAD
# simulate command; they are not part of the automated test suite
# (they require Bullet support, built databases, ffmpeg, and a Python
# interpreter with access to the BRL-CAD build tree).
#
# See README.md for usage instructions.

cmakefiles(
  CMakeLists.txt
  README.md
  cinematic_render.py
  cinematic_setup.tcl
  cinematic_sim.sh
  render_frames.py
  render_sample.png
  resume_cinematic.sh
  run_sim.sh
  setup_sim.tcl
  truck_terrain_simulation.mp4
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
