!*--------------------------------------------------------------------*!
!* Title:   creo_brl.res
!* Purpose: Dialog box resource file for Creo to BRL-CAD Converter
!* Version: Created for use with Creo version 7.0.9.0
!* Date:    3/22/2023
!*
!* Notes:
!*  1) Refer to: ..\src\external\Creo\main.cpp for more information
!*  2) ProToolkit function calls within "main.cpp" rely on this file
!*  3) Summary of ProToolkit component name reliance:
!*
!*         ProToolkit Function             Component Name
!*     ---------------------------------------------------
!*      ProUICheckbuttonActivateActionSet   CheckButton
!*      ProUICheckbuttonGetState            CheckButton
!*      ProUIDialogActivate                 Dialog
!*      ProUIDialogCreate                   Dialog
!*      ProUIInputpanelEditable             InputPanel
!*      ProUIInputpanelMaxlenSet            InputPanel
!*      ProUIInputpanelReadOnly             InputPanel
!*      ProUIInputpanelValueGet             InputPanel
!*      ProUILabelTextSet                   Label
!*      ProUIRadiogroupSelectednamesGet     RadioGroup
!*     ---------------------------------------------------
!*
!*  4) Upon installation, locate this resource file here:
!*
!*      C:\Program Files\PTC\Creo {version}\Common Files\text\resource
!*
!*  5) Establish a mono-space UI font with the following setting:
!*
!*      "default_ui_font                    consolas, bold"
!*
!*  6) Place the above setting in the "config.pro" file located here:
!*
!*      C:\Program Files\PTC\Creo {version}\Common Files\text\config.pro
!*
!*--------------------------------------------------------------------*!

(Dialog creo_brl
    (Components
        (SubLayout   data_controls)
        (Separator   separator1)
        (SubLayout   transform_controls)
        (Separator   separator2)
        (SubLayout   process_controls)
        (SubLayout   small_feature_controls)
        (SubLayout   tessellation_controls)
        (SubLayout   surface_controls)
        (Separator   separator3)
        (SubLayout   action_buttons)
        (Separator   separator4)
        (SubLayout   conversion_status)
    )
    (Resources
        (separator1.BottomOffset       4)
        (separator1.LeftOffset         10)
        (separator1.RightOffset        10)
        (separator1.TopOffset          4)
        (separator2.BottomOffset       4)
        (separator2.LeftOffset         10)
        (separator2.RightOffset        10)
        (separator2.TopOffset          4)
        (separator3.BottomOffset       4)
        (separator3.LeftOffset         10)
        (separator3.RightOffset        10)
        (separator3.TopOffset          4)
        (separator4.BottomOffset       4)
        (separator4.LeftOffset         10)
        (separator4.RightOffset        10)
        (separator4.TopOffset          4)
        (.DefaultButton                "doit")
        (.DialogStyle                  6)
        (.Label                        "Creo to BRL-CAD Converter (7.0.9.0)")
        (.Resizeable                   False)
        (.StartLocation                3)
        (.Layout
            (Grid (Rows 1 1 1 1 1 1 1 1 1) (Cols 1)
                data_controls
                separator1
                transform_controls
                separator2
                (Grid (Rows 1) (Cols 1 1) 
                    (Grid (Rows 1 1) (Cols 1) 
                        process_controls
                        tessellation_controls
                    )
                    (Grid (Rows 1 1) (Cols 1) 
                        small_feature_controls
                        surface_controls
                    )
                )
                separator3
                action_buttons
                separator4
                conversion_status
            )
        )
    )
)

(Layout data_controls
    (Components
        (Label        out_fname_l)
        (InputPanel   out_fname)
        (Label        log_file_type_l)
        (RadioGroup   log_file_type_rg)
        (Label        log_fname_l)
        (InputPanel   log_fname)
        (Label        mtl_fname_l)
        (InputPanel   mtl_fname)
        (Label        empty_label)
        (Label        parameter_label)
        (Label        param_rename_l)
        (InputPanel   param_rename)
        (Label        param_save_l)
        (InputPanel   param_save)
    )
    (Resources
        (out_fname_l.AttachRight       True)
        (out_fname_l.Label             "Output file name:")
        (out_fname_l.LeftOffset        4)
        (out_fname_l.RightOffset       0)
        (out_fname_l.TopOffset         8)
        (out_fname.AttachLeft          True)
        (out_fname.HelpText            "Enter name of BRL-CAD output file (creo.g)")
        (out_fname.LeftOffset          0)
        (out_fname.RightOffset         4)
        (out_fname.TopOffset           8)
        (out_fname.Value               "creo.g")
        (log_file_type_l.AttachRight   True)
        (log_file_type_l.Label         "Process log criteria:")
        (log_file_type_l.LeftOffset    4)
        (log_file_type_l.RightOffset   0)
        (log_file_type_rg.Alignment    0)
        (log_file_type_rg.AttachLeft   True)
        (log_file_type_rg.Labels       "Failure"
                                       "Success"
                                       "Failure or Success"
                                       "All (Debug)")
        (log_file_type_rg.Names        "Failure"
                                       "Success"
                                       "Failure/Success"
                                       "All/(Debug)")
        (log_file_type_rg.LeftOffset   0)
        (log_file_type_rg.Orientation  False)
        (log_file_type_rg.RightOffset  4)
        (log_fname_l.AttachRight       True)
        (log_fname_l.Label             "Process log file name:")
        (log_fname_l.LeftOffset        4)
        (log_fname_l.RightOffset       0)
        (log_fname.AttachLeft          True)
        (log_fname.HelpText            "Enter a file name here to create a process log file")
        (log_fname.LeftOffset          0)
        (log_fname.RightOffset         4)
        (log_fname.Value               "")
        (mtl_fname_l.AttachRight       True)
        (mtl_fname_l.Label             "Material file name:")
        (mtl_fname_l.LeftOffset        4)
        (mtl_fname_l.RightOffset       0)
        (mtl_fname.AttachLeft          True)
        (mtl_fname.HelpText            "Enter a file name here for the material translation file (optional)")
        (mtl_fname.LeftOffset          0)
        (mtl_fname.RightOffset         4)
        (mtl_fname.Value               "./ptc_materials.mtl")
        (empty_label.AttachRight       True)
        (empty_label.Label             "")
        (parameter_label.AttachLeft    True)
        (parameter_label.Label         "Creo Parameters")
        (parameter_label.LeftOffset    150)
        (param_rename_l.AttachRight    True)
        (param_rename_l.Label          "Create object names:")
        (param_rename_l.LeftOffset     4)
        (param_rename_l.RightOffset    0)
        (param_rename.AttachLeft       True)
        (param_rename.HelpText         "Enter a comma-separated list of Creo parameters which create BRL-CAD object names (optional)")
        (param_rename.LeftOffset       0)
        (param_rename.RightOffset      4)
        (param_rename.Value            "nomenclature")
        (param_save_l.AttachRight      True)
        (param_save_l.Label            "Preserve as attributes:")
        (param_save_l.LeftOffset       4)
        (param_save_l.RightOffset      0)
        (param_save.AttachLeft         True)
        (param_save.HelpText           "Enter a comma-separated list of Creo parameters to preserve in the BRL-CAD .g file (optional)")
        (param_save.LeftOffset         0)
        (param_save.RightOffset        4)
        (param_save.Value              "nomenclature,part_name")
        (.AttachTop                    True)
        (.BottomOffset                 4)
        (.Decorated                    False)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1 1 1 1 1 1 1) (Cols 1 1)
                out_fname_l
                out_fname
                log_file_type_l
                log_file_type_rg
                log_fname_l
                log_fname
                mtl_fname_l
                mtl_fname
                empty_label
                parameter_label
                param_rename_l
                param_rename
                param_save_l
                param_save
            )
        )
    )
)

(Layout transform_controls
    (Components
        (Label        transform_l)
        (RadioGroup   transform_rg)
    )
    (Resources
        (transform_l.AttachRight       True)
        (transform_l.Label             "Coordinate Transformation:")
        (transform_l.LeftOffset        4)
        (transform_l.RightOffset       4)
        (transform_rg.Alignment        0)
        (transform_rg.AttachLeft       True)
        (transform_rg.Labels           "None"
                                       "x to z"
                                       "y to z")
        (transform_rg.Names            "none"
                                       "x_to_z"
                                       "y_to_z")
        (transform_rg.LeftOffset       0)
        (transform_rg.Orientation      False)
        (transform_rg.RightOffset      4)
        (.AttachTop                    True)
        (.BottomOffset                 2)
        (.Decorated                    False)
        (.TopOffset                    2)
        (.Layout
            (Grid (Rows 1 ) (Cols 1 1)
                transform_l
                transform_rg
            )
        )
    )
)

(Layout process_controls
    (Components
        (Label        region_counter_l)
        (InputPanel   region_counter)
        (Label        min_luminance_l)
        (InputPanel   min_luminance)
    )
    (Resources
        (region_counter_l.AttachRight  True)
        (region_counter_l.Label        "Initial region counter:")
        (region_counter_l.LeftOffset   16)
        (region_counter_l.RightOffset  0)
        (region_counter.AttachLeft     True)
        (region_counter.HelpText       "Enter desired value for intial region counter")
        (region_counter.InputType      2)
        (region_counter.LeftOffset     0)
        (region_counter.RightOffset    2)
        (region_counter.Value          "1000")
        (min_luminance_l.AttachRight   True)
        (min_luminance_l.Label         "Min luminance, %:")
        (min_luminance_l.LeftOffset    2)
        (min_luminance_l.RightOffset   0)
        (min_luminance.AttachLeft      True)
        (min_luminance.HelpText        "Enter desired minimum luminance threshold [0 to 100] percent")
        (min_luminance.InputType       2)
        (min_luminance.LeftOffset      0)
        (min_luminance.RightOffset     2)
        (min_luminance.Value           "30")
        (.Alignment                    0)
        (.AttachLeft                   True)
        (.AttachTop                    True)
        (.Decorated                    True)
        (.Label                        "Process Controls")
        (.LeftOffset                   10)
        (.RightOffset                  2)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1 1) (Cols 1 1)
                region_counter_l
                region_counter
                min_luminance_l
                min_luminance
            )
        )
    )
)

(Layout tessellation_controls
    (Components
        (Label        max_chord_l)
        (InputPanel   max_chord)
        (Label        min_chord_l)
        (InputPanel   min_chord)
        (Label        max_angle_l)
        (InputPanel   max_angle)
        (Label        min_angle_l)
        (InputPanel   min_angle)
        (Label        max_steps_l)
        (InputPanel   max_steps)
    )
    (Resources
        (max_chord_l.AttachRight       True)
        (max_chord_l.Label             "Max chord height, mm:")
        (max_chord_l.LeftOffset        2)
        (max_chord_l.RightOffset       0)
        (max_chord.AttachLeft          True)
        (max_chord.HelpText            "Enter maximum chord height for tessellation of curved surfaces")
        (max_chord.InputType           3)
        (max_chord.LeftOffset          0)
        (max_chord.RightOffset         2)
        (max_chord.Value               "0.0")
        (min_chord_l.AttachRight       True)
        (min_chord_l.Label             "Min chord height, mm:")
        (min_chord_l.LeftOffset        2)
        (min_chord_l.RightOffset       0)
        (min_chord.AttachLeft          True)
        (min_chord.HelpText            "Enter minimum chord height for tessellation of curved surfaces")
        (min_chord.InputType           3)
        (min_chord.LeftOffset          0)
        (min_chord.RightOffset         2)
        (min_chord.Value               "0.0")
        (max_angle_l.AttachRight       True)
        (max_angle_l.Label             "Max angle control:")
        (max_angle_l.LeftOffset        2)
        (max_angle_l.RightOffset       0)
        (max_angle.AttachLeft          True)
        (max_angle.HelpText            "Larger values here produce finer tessellations [0.0 to 1.0]")
        (max_angle.InputType           3)
        (max_angle.LeftOffset          0)
        (max_angle.RightOffset         2)
        (max_angle.Value               "1.0")
        (min_angle_l.AttachRight       True)
        (min_angle_l.Label             "Min angle control:")
        (min_angle_l.LeftOffset        2)
        (min_angle_l.RightOffset       0)
        (min_angle.AttachLeft          True)
        (min_angle.HelpText            "Larger values here produce finer tessellations [0.0 to 1.0]")
        (min_angle.InputType           3)
        (min_angle.LeftOffset          0)
        (min_angle.RightOffset         2)
        (min_angle.Value               "0.1")
        (max_steps_l.AttachRight       True)
        (max_steps_l.Label             "Maximum number of steps:")
        (max_steps_l.LeftOffset        2)
        (max_steps_l.RightOffset       0)
        (max_steps.AttachLeft          True)
        (max_steps.HelpText            "Enter maximum number of tessellation attempts")
        (max_steps.InputType           2)
        (max_steps.LeftOffset          0)
        (max_steps.RightOffset         2)
        (max_steps.Value               "50")
        (.Alignment                    0)
        (.AttachLeft                   True)
        (.AttachTop                    True)
        (.Decorated                    True)
        (.Label                        "Tessellation Controls")
        (.LeftOffset                   10)
        (.RightOffset                  2)
        (.TopOffset                    1)
        (.Layout
            (Grid (Rows 1 1 1 1 1) (Cols 1 1)
                max_chord_l
                max_chord
                min_chord_l
                min_chord
                max_angle_l
                max_angle
                min_angle_l
                min_angle
                max_steps_l
                max_steps
            )
        )
    )
)

(Layout small_feature_controls
    (Components
        (CheckButton   elim_small)
        (Label         min_hole_l)
        (InputPanel    min_hole)
        (Label         min_chamfer_l)
        (InputPanel    min_chamfer)
        (Label         min_round_l)
        (InputPanel    min_round)
    )
    (Resources
        (elim_small.AttachRight        True)
        (elim_small.AttachTop          True)
        (elim_small.BottomOffset       0)
        (elim_small.HelpText           "Select this to ignore holes, rounds, and chamfers smaller than the specified minimums")
        (elim_small.Label              "Ignore minimum sizes")
        (elim_small.RightOffset        10)
        (elim_small.Set                False)
        (elim_small.TopOffset          0)
        (min_hole_l.AttachRight        True)
        (min_hole_l.AttachTop          True)
        (min_hole_l.BottomOffset       0)
        (min_hole_l.Label              "Hole diameter, mm:")
        (min_hole_l.LeftOffset         2)
        (min_hole_l.RightOffset        0)
        (min_hole_l.TopOffset          2)
        (min_hole.AttachLeft           True)
        (min_hole.AttachTop            True)
        (min_hole.BottomOffset         0)
        (min_hole.Editable             False)
        (min_hole.HelpText             "Any holes with diameter less than this value will be ignored")
        (min_hole.InputType            3)
        (min_hole.LeftOffset           0)
        (min_hole.RightOffset          2)
        (min_hole.TopOffset            0)
        (min_hole.Value                "0.0")
        (min_chamfer_l.AttachRight     True)
        (min_chamfer_l.AttachTop       True)
        (min_chamfer_l.BottomOffset    0)
        (min_chamfer_l.Label           "Chamfer dimension, mm:")
        (min_chamfer_l.LeftOffset      2)
        (min_chamfer_l.RightOffset     0)
        (min_chamfer_l.TopOffset       0)
        (min_chamfer.AttachLeft        True)
        (min_chamfer.AttachTop         True)
        (min_chamfer.BottomOffset      0)
        (min_chamfer.Editable          False)
        (min_chamfer.HelpText          "Any chamfers with dimensions less than this will be ignored")
        (min_chamfer.InputType         3)
        (min_chamfer.LeftOffset        0)
        (min_chamfer.RightOffset       2)
        (min_chamfer.TopOffset         0)
        (min_chamfer.Value             "0.0")
        (min_round_l.AttachRight       True)
        (min_round_l.AttachTop         True)
        (min_round_l.BottomOffset      0)
        (min_round_l.Label             "Blend radius, mm:")
        (min_round_l.LeftOffset        2)
        (min_round_l.RightOffset       0)
        (min_round_l.TopOffset         0)
        (min_round.AttachLeft          True)
        (min_round.AttachTop           True)
        (min_round.BottomOffset        0)
        (min_round.Editable            False)
        (min_round.HelpText            "Any blends or rounds with radius less than this will be ignored")
        (min_round.InputType           3)
        (min_round.LeftOffset          0)
        (min_round.RightOffset         2)
        (min_round.TopOffset           0)
        (min_round.Value               "0.0")
        (.Alignment                    0)
        (.AttachLeft                   True)
        (.AttachTop                    True)
        (.Decorated                    True)
        (.Label                        "Small Feature Controls")
        (.LeftOffset                   2)
        (.RightOffset                  10)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1 1) (Cols 1)
                elim_small
                (Grid (Rows 1 1 1) (Cols 1 1)
                    min_hole_l
                    min_hole
                    min_chamfer_l
                    min_chamfer
                    min_round_l
                    min_round
                )
            )
        )
    )
)

(Layout surface_controls
    (Components
        (CheckButton   facets_only)
        (CheckButton   check_solidity)
        (CheckButton   create_boxes)
        (CheckButton   get_normals)
    )
    (Resources
        (facets_only.AttachLeft        True)
        (facets_only.AttachTop         True)
        (facets_only.BottomOffset      0)
        (facets_only.HelpText          "Select this to produce a faceted approximation for each region with no CSG")
        (facets_only.Label             "Facetize everything, (no CSG)")
        (facets_only.LeftOffset        0)
        (facets_only.RightOffset       88)
        (facets_only.Set               True)
        (facets_only.TopOffset         0)
        (check_solidity.AttachLeft     True)
        (check_solidity.AttachTop      True)
        (check_solidity.BottomOffset   0)
        (check_solidity.HelpText       "Select this to reject BoTs that fail the solidity test (no unmated edges)")
        (check_solidity.Label          "Reject BoTs that fail solidity")
        (check_solidity.LeftOffset     0)
        (check_solidity.RightOffset    88)
        (check_solidity.Set            True)
        (check_solidity.TopOffset      1)
        (create_boxes.AttachLeft       True)
        (create_boxes.AttachTop        True)
        (create_boxes.BottomOffset     0)
        (create_boxes.HelpText         "Select this to create a bounding box solid from the Creo part upon part tessellation failure")
        (create_boxes.Label            "Bounding box replaces failed part")
        (create_boxes.LeftOffset       0)
        (create_boxes.RightOffset      88)
        (create_boxes.Set              True)
        (create_boxes.TopOffset        1)
        (get_normals.AttachLeft        True)
        (get_normals.AttachTop         True)
        (get_normals.BottomOffset      0)
        (get_normals.HelpText          "Select this to save surface normals in BoTs (raytracing looks smoother)")
        (get_normals.Label             "Write surface normals")
        (get_normals.LeftOffset        0)
        (get_normals.RightOffset       88)
        (get_normals.Set               False)
        (get_normals.TopOffset         1)
        (.Alignment                    0)
        (.AttachLeft                   True)
        (.AttachTop                    True)
        (.Decorated                    True)
        (.Label                        "Surface Controls")
        (.LeftOffset                   2)
        (.RightOffset                  10)
        (.TopOffset                    1)
        (.Layout
            (Grid (Rows 1 1 1 1) (Cols 1)
                facets_only
                check_solidity
                create_boxes
                get_normals
            )
        )
    )
)

(Layout action_buttons
    (Components
        (PushButton   doit)
        (PushButton   quit)
    )
    (Resources
        (doit.AttachRight              True)
        (doit.HelpText                 "Start converting the currently displayed object")
        (doit.Label                    "Convert")
        (quit.AttachLeft               True)
        (quit.HelpText                 "Close this window")
        (quit.Label                    "Quit")
        (.AttachTop                    True)
        (.BottomOffset                 2)
        (.TopOffset                    2)
        (.Layout
            (Grid (Rows 1) (Cols 1 1)
                doit
                quit
            )
        )
    )
)

(Layout conversion_status
    (Components
        (Label   conv_status)
    )
    (Resources
        (conv_status.Label             "                          Status: Waiting for input...                          ")
        (conv_status.Width             150)
        (.AttachTop                    True)
        (.BottomOffset                 2)
        (.LeftOffset                   4)
        (.RightOffset                  4)
        (.TopOffset                    2)
        (.Layout
            (Grid (Rows 1) (Cols 1)
                conv_status
            )
        )
    )
)
