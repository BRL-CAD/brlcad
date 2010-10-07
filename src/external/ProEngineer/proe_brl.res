(Dialog proe_brl
	(Components
		(Label		output_file_l)
		(InputPanel	output_file)
		(Label		starting_ident_l)
		(InputPanel	starting_ident)
		(Label		max_error_l)
		(InputPanel	max_error)
		(Label		min_error_l)
		(InputPanel	min_error)
		(Label		max_angle_ctrl_l)
		(InputPanel	max_angle_ctrl)
		(Label		min_angle_ctrl_l)
		(InputPanel	min_angle_ctrl)
		(Label		isteps_l)
		(InputPanel	isteps)
		(Label		log_file_l)
		(InputPanel	log_file)
		(CheckButton	log_success)
		(CheckButton	log_other)
		(Label		log_file_type_l)
		(RadioGroup	log_file_type_rg)
		(Label		name_file_l)
		(InputPanel	name_file)
		(Label		curr_proc_l)
		(Label		curr_proc)
		(CheckButton	facets_only)
		(CheckButton	elim_small)
		(CheckButton	get_normals)
		(Label		min_hole_l)
		(InputPanel	min_hole)
		(Label		min_chamfer_l)
		(InputPanel	min_chamfer)
		(Label		min_round_l)
		(InputPanel	min_round)
		(PushButton	doit)
		(PushButton	quit)
		(Separator	separator1)
		(Separator	separator2)
		(Separator	separator3)
		(Separator	separator4)
		(Separator	separator5)
		(Separator	separator6)
	)
	(Resources
		(separator1.TopOffset		4)
		(separator1.BottomOffset	4)
		(separator2.TopOffset		4)
		(separator2.BottomOffset	4)
		(separator3.TopOffset		4)
		(separator3.BottomOffset	4)
		(separator4.TopOffset		4)
		(separator4.BottomOffset	4)
		(separator5.TopOffset		4)
		(separator5.BottomOffset	4)
		(separator6.TopOffset		4)
		(separator6.BottomOffset	4)
		(name_file_l.Label		"Part Name File: ")
		(name_file_l.AttachRight	True)
		(name_file.Value		"")
		(name_file.HelpText		"Enter the name of a part number to part name mapping file (optional)")
		(log_file_type_l.Label		"Output Log File Type: ")
		(log_file_type_l.AttachRight	True)
		(log_file_type_rg.Names		"Failure"
						"Success"
						"Failure/Success"
						"All")
		(log_file_type_rg.Labels	"Failure"
						"Success"
						"Failure and Success"
						"All (Debug)")
		(log_file_type_rg.Orientation	False)
		(log_file_type_rg.AttachLeft	True)
		(log_file_l.Label		"Output Log File Name: ")
		(log_file_l.AttachRight		True)
		(log_file.Value			"")
		(log_file.HelpText		"Enter a file name here to receive verbose debugging output, or leave blank")
		(curr_proc_l.Label		"Current status: ")
		(curr_proc.Columns		30)
		(curr_proc.Label		"Not Yet Processing")
		(curr_proc.AttachLeft		True)
		(output_file_l.Label		"Output File Name: " )
		(output_file_l.AttachRight	True)
		(output_file.Value		"proe.asc")
		(output_file.HelpText		"Enter name of file to receive ASCII output here")
		(starting_ident_l.Label		"Starting Identifier (Ident #): ")
		(starting_ident_l.AttachRight	True)
		(starting_ident.InputType	2)
		(starting_ident.Value		"1000")
		(starting_ident.HelpText	"Enter desired value for first region identifier")
		(max_error_l.Label		"Max tesellation error (mm): ")
		(max_error_l.AttachRight	True)
		(max_error.InputType		3)
		(max_error.Value		"3.0")
		(max_error.HelpText		"Enter maximum chord error (in mm) for tessellation of curved surfaces")
		(min_error_l.Label		"Min tesellation error (mm): ")
		(min_error_l.AttachRight	True)
		(min_error.InputType		3)
		(min_error.Value		"3.0")
		(min_error.HelpText		"Enter minimum chord error (in mm) for tessellation of curved surfaces")
		(max_angle_ctrl_l.Label		"Max Angle Control Value: ")
		(max_angle_ctrl_l.AttachRight	True)
		(max_angle_ctrl.InputType	3)
		(max_angle_ctrl.Value		"0.5")
		(max_angle_ctrl.HelpText	"(0.0 - 1.0) larger values here produce finer tesselations")
		(min_angle_ctrl_l.Label		"Min Angle Control Value: ")
		(min_angle_ctrl_l.AttachRight	True)
		(min_angle_ctrl.InputType	3)
		(min_angle_ctrl.Value		"0.5")
		(min_angle_ctrl.HelpText	"(0.0 - 1.0) larger values here produce finer tesselations")
		(isteps_l.Label			"Max to Min Control Steps: ")
		(isteps_l.AttachRight		True)
		(isteps.InputType		2)
		(isteps.Value			"0")
		(isteps.HelpText		"Enter number of steps between the min and max tolerance")
		(facets_only.Label		"Facetize everything (Evaluate booleans, no CSG)")
		(facets_only.HelpText		"Select this to produce a faceted approximation for each region with no CSG")
		(facets_only.Set		True)
		(facets_only.AttachLeft		True)
		(elim_small.Label		"Ignore small features")
		(elim_small.HelpText		"Select this to ignore holes, rounds, and chamfers smaller than the specified minimums")
		(elim_small.Set			False)
		(get_normals.Label		"Write surface normals")
		(get_normals.Set		False)
		(get_normals.HelpText		"Select this to save surface normals in BoTs (raytracing looks smoother)")
		(get_normals.AttachLeft		True)
		(min_hole_l.Label		"Minimum hole diameter (mm): ")
		(min_hole_l.AttachRight		True)
		(min_hole.InputType		3)
		(min_hole.Value			"0.0")
		(min_hole.Editable		False)
		(min_hole.HelpText		"Any holes with diameter less than this value will be ignored")
		(min_chamfer_l.Label		"Minimum chamfer dimension (mm): ")
		(min_chamfer_l.AttachRight	True)
		(min_chamfer.InputType		3)
		(min_chamfer.Value		"0.0")
		(min_chamfer.Editable		False)
		(min_chamfer.HelpText		"Any chamfers with dimensions less than this will be ignored")
		(min_round_l.Label		"Minimum round radius (mm): ")
		(min_round_l.AttachRight	True)
		(min_round.InputType		3)
		(min_round.Value		"0.0")
		(min_round.Editable		False)
		(min_round.HelpText		"Any rounds or blends with radius less than this will be ignored")
		(doit.Label			"Go")
		(doit.HelpText			"Start converting the currently displayed object")
		(quit.Label			"Dismiss")
		(quit.HelpText			"Close this window")
		(.DefaultButton			"doit")
		(.StartLocation			3)
		(.Resizeable			True)
		(.DialogStyle			6)
		(.Label				"Pro/E to BRL-CAD Converter")
		(.Layout
			(Grid (Rows 1 1 1 1 1 1 1) (Cols 1)
				(Grid (Rows 1 1 1 1 1 1 1 1 1 1) (Cols 1 1)
					output_file_l
					output_file
					log_file_type_l
					log_file_type_rg
					log_file_l
					log_file
					name_file_l
					name_file
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
				separator1
				facets_only
				get_normals
				separator3
				elim_small
				(Grid (Rows 1 1 1 1 1 1) (Cols 1 1)
					min_hole_l
					min_hole
					min_chamfer_l
					min_chamfer
					min_round_l
					min_round
					separator5
					separator6
					doit
					quit
					curr_proc_l
					curr_proc
				)
			)
		)
	)
)
