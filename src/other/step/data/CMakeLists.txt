SET(ap203_data
  ap203/cube1.p21
  ap203/cube2.p21
  ap203/g_r_assembly1.p21
  ap203/gasket1.p21
  ap203/gasket2.p21
  ap203/gasket3.p21
  ap203/hex_prism1.p21
  ap203/rod_aspect1.p21
  ap203/star1.p21
)
INSTALL(FILES ${ap203_data} DESTINATION ${DATA_DIR}/data/ap203)

SET(ap227_data
  ap227/mitre.p21
  ap227/mitre.step.txt
)
INSTALL(FILES ${ap227_data} DESTINATION ${DATA_DIR}/data/ap227)

SET(step_datafiles
  203wseds.exp
  ap201.exp
  ap202.exp
  ap203edit.exp
  ap225.exp
  ap227.exp
  pdmnet.exp
  select.exp
  wg3n1063_ap232.exp
  wg3n1342_ap218.exp
  wg3n1469_ap240.exp
  wg3n1568_ap239.exp
  wg3n1598_ap221.exp
  wg3n1941_ap224e3.exp
  wg3n2097_ap238.exp
  wg3n2188_ap236.exp
  wg3n2253_ap219_lf.exp
  wg3n2366_ap216tc1_aim_lf.exp
  wg3n2392_ap215tc1_aim_lf.exp
  wg3n2405_ap227e2tc1_aim_lf.exp
  wg3n2408_ap223_aim_lf.exp
  wg3n2545_ap235_aim_lf.exp
  wg3n2601_ap210e2_aim_lf.exp
  wg3n2628_ap214e3.exp
  wg3n2635_ap203e2_aim_lf.exp
  wg3n916_ap203.exp
  wg3n972_ap209.exp
)
INSTALL(FILES ${step_datafiles} DESTINATION ${DATA_DIR}/data)
