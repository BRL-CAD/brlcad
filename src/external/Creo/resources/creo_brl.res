!*--------------------------------------------------------------------*!
!* Title:   creo_brl.res
!* Purpose: Dialog Box Resource file for Creo to BRL-CAD Converter
!* Version: Created for use with Creo version 7.0.6.0
!* Date:    4/15/2022
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
!*       C:\Program Files\PTC\Creo {version}\Common Files\text\resource
!*
!*--------------------------------------------------------------------*!

(Dialog creo_brl
    (Components
        (SubLayout   main_inputs)
        (Separator   separator1)
        (SubLayout   process_controls)
        (Separator   separator2)
        (SubLayout   small_features)
        (Separator   separator3)
        (SubLayout   action_buttons)
        (Separator   separator4)
        (SubLayout   conversion_status)
    )
    (Resources
        (.DefaultButton                 "doit")
        (.DialogStyle                   6)
        (.Label                         "Creo to BRL-CAD Converter (7.0.6.0)")
        (.Resizeable                    False)
        (.StartLocation                 3)
        (separator1.BottomOffset        8)
        (separator1.LeftOffset         10)
        (separator1.RightOffset        10)
        (separator1.TopOffset           8)
        (separator2.BottomOffset        8)
        (separator2.LeftOffset         10)
        (separator2.RightOffset        10)
        (separator2.TopOffset           8)
        (separator3.BottomOffset        8)
        (separator3.LeftOffset         10)
        (separator3.RightOffset        10)
        (separator3.TopOffset           8)
        (separator4.BottomOffset        8)
        (separator4.LeftOffset         10)
        (separator4.RightOffset        10)
        (separator4.TopOffset           8)
        (.Layout
            (Grid (Rows 1 1 1 1 1 1 1) (Cols 1)
                main_inputs
                separator1
                (Grid (Rows 1) (Cols 1 1)
                    process_controls
                    small_features
                )
                separator2
                action_buttons
                separator3
                conversion_status
            )
        )
    )
)

(Layout main_inputs
    (Components
        (Label        output_file_l)
        (InputPanel   output_file)
        (Label        log_file_type_l)
        (RadioGroup   log_file_type_rg)
        (Label        log_file_l)
        (InputPanel   log_file)
        (Label        attr_rename_l)
        (InputPanel   attr_rename)
        (Label        attr_save_l)
        (InputPanel   attr_save)
        (Label        starting_ident_l)
        (InputPanel   starting_ident)
        (Label        max_error_l)
        (InputPanel   max_error)
        (Label        min_error_l)
        (InputPanel   min_error)
        (Label        max_angle_ctrl_l)
        (InputPanel   max_angle_ctrl)
        (Label        min_angle_ctrl_l)
        (InputPanel   min_angle_ctrl)
        (Label        isteps_l)
        (InputPanel   isteps)
    )

    (Resources
        (output_file_l.AttachRight     True)
        (output_file_l.Label           "Output file name: ")
        (output_file_l.LeftOffset      10)
        (output_file_l.RightOffset      0)
        (output_file_l.TopOffset        8)
        (output_file.AttachLeft        True)
        (output_file.HelpText          "Enter name of BRL-CAD output file (creo.g)")
        (output_file.LeftOffset         0)
        (output_file.RightOffset       10)
        (output_file.TopOffset          8)
        (output_file.Value             "creo.g")
        (log_file_type_l.AttachRight   True)
        (log_file_type_l.Label         "Process log criteria: ")
        (log_file_type_l.LeftOffset    10)
        (log_file_type_l.RightOffset    0)
        (log_file_type_rg.Alignment     0)
        (log_file_type_rg.AttachLeft   True)
        (log_file_type_rg.Labels       "Failure"
                                       "Success"
                                       "Failure or Success"
                                       "All (Debug)")
        (log_file_type_rg.Names        "Failure"
                                       "Success"
                                       "Failure/Success"
                                       "All/(Debug)")
        (log_file_type_rg.LeftOffset    0)
        (log_file_type_rg.Orientation  False)
        (log_file_type_rg.RightOffset  10)
        (log_file_l.AttachRight        True)
        (log_file_l.Label              "Process log file name: ")
        (log_file_l.LeftOffset         10)
        (log_file_l.RightOffset         0)
        (log_file.AttachLeft           True)
        (log_file.HelpText             "Enter a file name here to create a process log file (optional)")
        (log_file.LeftOffset            0)
        (log_file.RightOffset          10)
        (log_file.Value                "")
        (attr_rename_l.AttachRight     True)
        (attr_rename_l.Label           "Parameters form object names: ")
        (attr_rename_l.LeftOffset      10)
        (attr_rename_l.RightOffset      0)
        (attr_rename.AttachLeft        True)
        (attr_rename.HelpText          "Enter a comma-separated list of Creo parameters which build BRL-CAD object names (optional)")
        (attr_rename.LeftOffset         0)
        (attr_rename.RightOffset       10)
        (attr_rename.Value             "")
        (attr_save_l.AttachRight       True)
        (attr_save_l.Label             "Parameters saved as attributes: ")
        (attr_save_l.LeftOffset        10)
        (attr_save_l.RightOffset        0)
        (attr_save.AttachLeft          True)
        (attr_save.HelpText            "Enter a comma-separated list of Creo parameters to preserve in the BRL-CAD .g file (optional)")
        (attr_save.LeftOffset           0)
        (attr_save.RightOffset         10)
        (attr_save.Value               "")
        (starting_ident_l.AttachRight  True)
        (starting_ident_l.Label        "Initial region counter: ")
        (starting_ident_l.LeftOffset   10)
        (starting_ident_l.RightOffset   0)
        (starting_ident.AttachLeft     True)
        (starting_ident.HelpText       "Enter desired value for intial region counter")
        (starting_ident.InputType      2)
        (starting_ident.LeftOffset      0)
        (starting_ident.RightOffset    10)
        (starting_ident.Value          "1000")
        (max_error_l.AttachRight       True)
        (max_error_l.Label             "Max tessellation error, mm: ")
        (max_error_l.LeftOffset        10)
        (max_error_l.RightOffset        0)
        (max_error.AttachLeft          True)
        (max_error.HelpText            "Enter maximum chord error for tessellation of curved surfaces")
        (max_error.InputType           3)
        (max_error.LeftOffset           0)
        (max_error.RightOffset         10)
        (max_error.Value               "1.0")
        (min_error_l.AttachRight       True)
        (min_error_l.Label             "Min tessellation error, mm: ")
        (min_error_l.LeftOffset        10)
        (min_error_l.RightOffset        0)
        (min_error.AttachLeft          True)
        (min_error.HelpText            "Enter minimum chord error for tessellation of curved surfaces")
        (min_error.InputType           3)
        (min_error.LeftOffset           0)
        (min_error.RightOffset         10)
        (min_error.Value               "0.1")
        (max_angle_ctrl_l.AttachRight  True)
        (max_angle_ctrl_l.Label        "Max angle control value, deg: ")
        (max_angle_ctrl_l.LeftOffset   10)
        (max_angle_ctrl_l.RightOffset   0)
        (max_angle_ctrl.AttachLeft     True)
        (max_angle_ctrl.HelpText       "(0.0 - 1.0) larger values here produce finer tessellations")
        (max_angle_ctrl.InputType      3)
        (max_angle_ctrl.LeftOffset      0)
        (max_angle_ctrl.RightOffset    10)
        (max_angle_ctrl.Value          "1.0")
        (min_angle_ctrl_l.AttachRight  True)
        (min_angle_ctrl_l.Label        "Min angle control value, deg: ")
        (min_angle_ctrl_l.LeftOffset   10)
        (min_angle_ctrl_l.RightOffset   0)
        (min_angle_ctrl.AttachLeft     True)
        (min_angle_ctrl.HelpText       "(0.0 - 1.0) larger values here produce finer tessellations")
        (min_angle_ctrl.InputType      3)
        (min_angle_ctrl.LeftOffset      0)
        (min_angle_ctrl.RightOffset    10)
        (min_angle_ctrl.Value          "1.0")
        (isteps_l.AttachRight          True)
        (isteps_l.Label                "Max to Min control steps: ")
        (isteps_l.LeftOffset           10)
        (isteps_l.RightOffset           0)
        (isteps.AttachLeft             True)
        (isteps.HelpText               "Enter number of steps between the min to max angle control value")
        (isteps.InputType              2)
        (isteps.LeftOffset              0)
        (isteps.RightOffset            10)
        (isteps.Value                  "20")
        (.AttachTop                    True)
        (.BottomOffset                 4)
        (.Decorated                    False)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1 1 1 1 1 1 1 1 1 1 1) (Cols 1 1)
                output_file_l
                output_file
                log_file_type_l
                log_file_type_rg
                log_file_l
                log_file
                attr_rename_l
                attr_rename
                attr_save_l
                attr_save
                starting_ident_l
                starting_ident
                max_error_l
                max_error
                min_error_l
                min_error
                max_angle_ctrl_l
                max_angle_ctrl
                min_angle_ctrl_l
                min_angle_ctrl
                isteps_l
                isteps
            )
        )
    )
)


(Layout process_controls
    (Components
        (CheckButton   facets_only)
        (CheckButton   get_normals)
        (CheckButton   check_solidity)
        (CheckButton   debug_bboxes)
    )

    (Resources
        (facets_only.AttachLeft        True)
        (facets_only.AttachTop         True)
        (facets_only.BottomOffset      0)
        (facets_only.HelpText          "Select this to produce a faceted approximation for each region with no CSG")
        (facets_only.Label             "Facetize everything, (no CSG)")
        (facets_only.LeftOffset        0)
        (facets_only.RightOffset       0)
        (facets_only.Set               True)
        (facets_only.TopOffset         0)
        (get_normals.AttachLeft        True)
        (get_normals.AttachTop         True)
        (get_normals.BottomOffset      0)
        (get_normals.HelpText          "Select this to save surface normals in BoTs (raytracing looks smoother)")
        (get_normals.Label             "Write surface normals")
        (get_normals.LeftOffset        0)
        (get_normals.RightOffset       0)
        (get_normals.Set               False)
        (get_normals.TopOffset         4)
        (check_solidity.AttachLeft     True)
        (check_solidity.AttachTop      True)
        (check_solidity.BottomOffset   0)
        (check_solidity.HelpText       "Select this to reject BoTs that fail the solidity test (no unmated edges)")
        (check_solidity.Label          "Reject BoTs that fail solidity")
        (check_solidity.LeftOffset     0)
        (check_solidity.RightOffset    0)
        (check_solidity.Set            False)
        (check_solidity.TopOffset      4)
        (debug_bboxes.AttachLeft       True)
        (debug_bboxes.AttachTop        True)
        (debug_bboxes.BottomOffset     0)
        (debug_bboxes.HelpText         "Select this to create a bounding box solid from the Creo part upon part tessellation failure")
        (debug_bboxes.Label            "Bounding box replaces failed part")
        (debug_bboxes.LeftOffset       0)
        (debug_bboxes.RightOffset      0)
        (debug_bboxes.Set              False)
        (debug_bboxes.TopOffset        4)
        (.Alignment                    0)
        (.AttachLeft                   True)
        (.AttachTop                    True)
        (.Decorated                    True)
        (.Label                        "Conversion Controls")
        (.LeftOffset                   10)
        (.RightOffset                  2)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1 1 1 1) (Cols 1)
                facets_only
                get_normals
                check_solidity
                debug_bboxes
            )
        )
    )
)

(Layout small_features
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
        (elim_small.RightOffset        4)
        (elim_small.Set                False)
        (elim_small.TopOffset          0)
        (min_hole_l.AttachRight        True)
        (min_hole_l.AttachTop          True)
        (min_hole_l.BottomOffset       0)
        (min_hole_l.Label              "Hole diameter, mm: ")
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
        (min_hole.RightOffset          4)
        (min_hole.TopOffset            0)
        (min_hole.Value                "0.0")
        (min_chamfer_l.AttachRight     True)
        (min_chamfer_l.AttachTop       True)
        (min_chamfer_l.BottomOffset    0)
        (min_chamfer_l.Label           "Chamfer dimension, mm: ")
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
        (min_chamfer.RightOffset       4)
        (min_chamfer.TopOffset         0)
        (min_chamfer.Value             "0.0")
        (min_round_l.AttachRight       True)
        (min_round_l.AttachTop         True)
        (min_round_l.BottomOffset      0)
        (min_round_l.Label             "Blend radius, mm: ")
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
        (min_round.RightOffset         4)
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
            (Grid (Rows 1 1 ) (Cols 1)
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

(Layout action_buttons
    (Components
        (PushButton   doit)
        (PushButton   quit)
    )

    (Resources
        (.AttachTop                    True)
        (.BottomOffset                 0)
        (.TopOffset                    0)
        (doit.AttachRight              True)
        (doit.HelpText                 "Start converting the currently displayed object")
        (doit.Label                    "Convert")
        (quit.AttachLeft               True)
        (quit.HelpText                 "Close this window")
        (quit.Label                    "Quit")
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
        (Label   conv_status_l)
        (Label   conv_status)
    )

    (Resources
        (conv_status_l.AttachRight     True)
        (conv_status_l.Label           "Status: ")
        (conv_status.AttachLeft        True)
        (conv_status.Label             "Waiting for input...")
        (conv_status.Width             -1)
        (.AttachTop                    True)
        (.BottomOffset                 4)
        (.TopOffset                    0)
        (.Layout
            (Grid (Rows 1) (Cols 1 1)
                conv_status_l
                conv_status
            )
        )
    )
)
