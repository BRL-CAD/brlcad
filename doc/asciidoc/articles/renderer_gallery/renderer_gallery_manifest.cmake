# renderer_gallery_manifest.cmake -- data table for the renderer showcase.
#
# Pure-CMake data file consumed by generate_renderer_gallery.cmake.  Each
# renderer style is one bracket-argument string in RENDERER_GALLERY_ITEM_<i>,
# with fields separated by "|":
#
#   name      image basename -> renderer_gallery_<name>.png
#   section   AsciiDoc section heading
#   tool      one of rt, rtedge, rtdepth, rtxray, rthide
#   output    one of png, bw, plot3
#   args      extra renderer arguments, parsed with separate_arguments()
#   title     display title
#   caption   description text shown in the article

set(RENDERER_GALLERY_ITEM_COUNT 17)

set(RENDERER_GALLERY_ITEM_0  [=[rt_l0|RT lighting models|rt|png|-l0|rt -l0 full lighting|Full lighting model with explicit Cornell-box light, material color, shadows, and reflection.]=])
set(RENDERER_GALLERY_ITEM_1  [=[rt_l1|RT lighting models|rt|png|-l1|rt -l1 eye diffuse|Simple diffuse lighting from the eye, useful for debugging object form without scene-light complexity.]=])
set(RENDERER_GALLERY_ITEM_2  [=[rt_l2|RT lighting models|rt|png|-l2|rt -l2 normals|Surface normals encoded as RGB colors, useful for checking orientation and curvature continuity.]=])
set(RENDERER_GALLERY_ITEM_3  [=[rt_l3|RT lighting models|rt|png|-l3|rt -l3 three-light|Three-light diffuse debug model. On the Cornell box this currently matches the full-lighting snapshot closely.]=])
set(RENDERER_GALLERY_ITEM_4  [=[rt_l4|RT lighting models|rt|png|-l4|rt -l4 curvature|Inverse-curvature debug display. The Cornell box is mostly planar, so the committed image is intentionally flat.]=])
set(RENDERER_GALLERY_ITEM_5  [=[rt_l5|RT lighting models|rt|png|-l5|rt -l5 principal direction|Principal curvature direction debug display.]=])
set(RENDERER_GALLERY_ITEM_6  [=[rt_l6|RT lighting models|rt|png|-l6|rt -l6 UV|UV coordinate test-map style; U appears in red and V in blue.]=])
set(RENDERER_GALLERY_ITEM_7  [=[rt_l7|RT lighting models|rt|png|-l7,1024,0,8,60.0,1,0,0,0,1.0,|rt -l7 photon map|Photon-mapping global illumination using a small deterministic photon count so regeneration remains quick enough for documentation snapshots.]=])
set(RENDERER_GALLERY_ITEM_8  [=[rt_l8|RT lighting models|rt|png|-l8|rt -l8 heat graph|Heat-graph mode. Current PNG file output matches the shaded image while heat timing is reported in the render log.]=])

set(RENDERER_GALLERY_ITEM_9  [=[rt_haze|RT output options|rt|png|-m 0.0008,0.55,0.72,0.92|rt haze|Full lighting with atmospheric haze enabled.]=])
set(RENDERER_GALLERY_ITEM_10 [=[rt_cutaway|RT output options|rt|png|-k 0,1,0,260|rt cut plane|Full lighting with a view-wide cutting plane applied at render time.]=])

set(RENDERER_GALLERY_ITEM_11 [=[rtedge_default|Edges and line renderers|rtedge|png||rtedge default|Default edge rendering on a dark background.]=])
set(RENDERER_GALLERY_ITEM_12 [=[rtedge_colored|Edges and line renderers|rtedge|png|-c "set fg=255/220/0 bg=20/22/26"|rtedge colored|Custom foreground and background colors.]=])
set(RENDERER_GALLERY_ITEM_13 [=[rtedge_regions|Edges and line renderers|rtedge|png|-c "set rc=1 dr=1"|rtedge region colors|Region-aware edge rendering using region colors.]=])
set(RENDERER_GALLERY_ITEM_14 [=[rthide|Edges and line renderers|rthide|plot3||rthide hidden line|Hidden-line plot3 output rasterized with BRL-CAD's plot3-fb and fb-png tools.]=])

set(RENDERER_GALLERY_ITEM_15 [=[rtdepth|Scalar image renderers|rtdepth|bw||rtdepth|Depth map rendered as grayscale.]=])
set(RENDERER_GALLERY_ITEM_16 [=[rtxray|Scalar image renderers|rtxray|bw||rtxray|Pseudo X-ray thickness image rendered as grayscale.]=])
