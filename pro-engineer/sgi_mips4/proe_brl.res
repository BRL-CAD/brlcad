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
		(Label		curr_proc_l)
		(Label		curr_proc)
		(PushButton	doit)
		(PushButton	quit)
	)
	(Resources
		(curr_proc_l.Label		"Currently processing:")
		(curr_proc_l.AttachRight	True)
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
		(max_error.Value		"1.5")
		(max_error.HelpText		"Enter maximum chord error (in mm) for tessellation of curved surfaces")
		(angle_ctrl_l.Label		"Angle Control Value:")
		(angle_ctrl.InputType		3)
		(angle_ctrl.Value		"0.5")
		(angle_ctrl.HelpText		"(0.0 - 1.0) larger values here produce finer tesselations")
		(doit.Label			"Go")
		(doit.HelpText			"Start converting the currently displayed object")
		(quit.Label			"Dismiss")
		(quit.HelpText			"Close this window")
		(.DefaultButton			"Go")
		(.StartLocation			3)
		(.Resizeable			True)
		(.DialogStyle			6)
		(.Label				"Pro/E to BRL-CAD Converter")
		(.Layout
			(Grid (Rows 1 1 1 1 1 1) (Cols 1 1)
				output_file_l
				output_file
				starting_ident_l
				starting_ident
				max_error_l
				max_error
				angle_ctrl_l
				angle_ctrl
				doit
				quit
				curr_proc_l
				curr_proc
			)
		)
	)
)
