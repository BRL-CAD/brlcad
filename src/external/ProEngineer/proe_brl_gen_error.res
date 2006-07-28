(Dialog proe_brl_gen_error
	(Components
		(TextArea	error_message)
		(PushButton	ok_button)
	)
	(Resources
		(ok_button.Label		"OK")
		(ok_button.HelpText		"Click on this to close this window" )
		(error_message.Editable		False)
		(error_message.Value		"ERROR")
		(error_message.Columns		30)
		(error_message.Rows		6)
		(.StartLocation			3)
		(.Resizeable			True)
		(.DialogStyle			6)
		(.Label				"Pro/E to BRL-CAD Converter Error")
		(.DefaultButton			"ok")
		(.Layout
			(Grid (Rows 1 1) (Cols 1)
				error_message
				ok_button
			)
		)
	)
)
