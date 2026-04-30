fname = sdrive.o lbfgs.o 

main : $(fname)
	f77 $(fname) $(extras) -o sdrive

.f.o :; f77 -c $*.f

