(Dialog proe_brl
	(Components
		(Label		output_file_l)
		(InputPanel	output_file)
		(Label		starting_ident_l)
		(InputPanel	starting_ident)
		(Label		max_error_l)
		(InputPanel	max_error)
		(Label		angle_ctrl_l)
		(InputPanel	angle_ctrl)
		(Label		log_file_l)
		(InputPanel	log_file)
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
		(name_file_l.Label		"Part Name File:")
		(name_file.Value		"")
		(name_file.HelpText		"Enter the name of a part number to part name mapping file (optional)")
		(log_file_l.Label		"Log File Name:")
		(log_file.Value			"")
		(log_file.HelpText		"Enter a file name here to receive verbose debugging output, or leave blank")
		(curr_proc_l.Label		"Current status:")
		(curr_proc.Columns		30)
		(curr_proc.Label		"Not Processing Yet")
		(curr_proc.AttachLeft		True)
		(output_file_l.Label		"Output File Name:" )
		(output_file.Value		"proe.asc")
		(output_file.HelpText		"Enter name of file to receive ASCII output here")
		(starting_ident_l.Label		"Starting Ident Number:")
		(starting_ident.InputType	2)
		(starting_ident.Value		"1000")
		(starting_ident.HelpText	"Enter desired value for first region ident here")
		(max_error_l.Label		"Max tesellation error(mm):")
		(max_error.InputType		3)
		(max_error.Value		"3.0")
		(max_error.HelpText		"Enter maximum chord error (in mm) for tessellation of curved surfaces")
		(angle_ctrl_l.Label		"Angle Control Value:")
		(angle_ctrl.InputType		3)
		(angle_ctrl.Value		"0.5")
		(angle_ctrl.HelpText		"(0.0 - 1.0) larger values here produce finer tesselations")
		(facets_only.Label		"Facetize everything (no CSG)")
		(facets_only.HelpText		"Select this to produce a faceted approximation for each region with no CSG")
		(facets_only.Set		True)
		(facets_only.AttachLeft		True)
		(elim_small.Label		"Eliminate small features")
		(elim_small.HelpText		"Select this to ignore holes, rounds, and chamfers smaller than the specified minimums")
		(elim_small.Set			False)
		(get_normals.Label		"Extract surface normals for facetted objects")
		(get_normals.Set		False)
		(get_normals.HelpText		"Select this to save surface normals in BOTs (raytracing looks smoother)")
		(get_normals.AttachLeft		True)
		(min_hole_l.Label		"Minimum hole diameter(mm):")
		(min_hole.InputType		3)
		(min_hole.Value			"0.0")
		(min_hole.Editable		False)
		(min_hole.HelpText		"Any holes with diameter less than this value will be ignored")
		(min_chamfer_l.Label		"Minimum chamfer dimension(mm):")
		(min_chamfer.InputType		3)
		(min_chamfer.Value		"0.0")
		(min_chamfer.Editable		False)
		(min_chamfer.HelpText		"Any chamfers with dimensions less than this will be ignored")
		(min_round_l.Label		"Minimum round radius(mm):")
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
				(Grid (Rows 1 1 1 1 1 1) (Cols 1 1)
					output_file_l
					output_file
					log_file_l
					log_file
					name_file_l
					name_file
					starting_ident_l
					starting_ident
					max_error_l
					max_error
					angle_ctrl_l
					angle_ctrl
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
