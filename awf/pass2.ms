/^\.\^b/ {			# initialization
	nhnos[1] = 0
	next
}
/^[^.]/ {			# text line -- reached only for input traps
	if (afternext == ",tP")
		print dobreak, "toindent"
	afternext = ""
	next
}
/^\.nH/ {			# fooling around for numbered headings
	no = 1
	if (NF > 1)
		no = $2
	if (no == 0) {
		nhnos[1] = 0
		no = 1
	}
	nhnos[no]++
	for (n in nhnos)
		if (n > no)
			nhnos[n] = 0
	s = ""
	for (n = 1; n <= no; n++)
		s = s nhnos[n] "."
	strings["Nh"] = s		# result in string for macro to grab
	next
}
/^\.ds/ {			# to catch special strings
	if ($3 !~ /^"/)
		v = $3
	else
		v = substr($0, index($0, "\"")+1)
	if ($2 ~ /^[LCR][HF]$/)
		print nobreak, $2, v
	# fallthrough to normal .ds processing in macro-independent stuff
}
/^\.\^e/ {			# finalization
	next
}
