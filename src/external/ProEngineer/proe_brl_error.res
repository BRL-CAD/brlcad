(Dialog proe_brl_error
	(Components
		(TextArea	the_message)
		(PushButton	ok)
	)
	(Resources
		(ok.Label			"OK")
		(ok.HelpText			"Click on this to close this window" )
		(the_message.Editable		False)
		(the_message.Value		"ERROR: model has been corrupted.
Please exit without saving")
		(the_message.Columns		30)
		(the_message.Rows		6)
		(.StartLocation			3)
		(.Resizeable			True)
		(.DialogStyle			6)
		(.Label				"Pro/E to BRL-CAD Converter Error")
		(.DefaultButton			"ok")
		(.Layout
			(Grid (Rows 1 1) (Cols 1)
				the_message
				ok
			)
		)
	)
)
