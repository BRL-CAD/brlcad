foreach(frmt ${OGR_FORMATS})
	add_subdirectory(${frmt})
endforeach(frmt ${OGR_FORMATS})
