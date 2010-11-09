proc bot_askforname {parent screen} {
   if {[cad_input_dialog $parent.botname $screen "BoT to Edit"\
	   "Enter name of BoT:" botname ""\
	   0 {{ summary "Object name of BoT to edit with BoT Editor"} { see_also bot_decimate }} OK Cancel] == 1} {
		   return
	   }
   BotEditor .botedit $botname
}


