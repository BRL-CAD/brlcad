#
# This file is a Makefile to compile all the encoding files.  
#
# Run "make" to compile all the encoding files (*.txt,*.esc) into the
# format that Tcl can use (*.enc).  It is your responsibility to move the
# encoding files to the appropriate place ($TCL_ROOT/library/encoding
#
# The .txt files in this directory come from the Unicode CD and are covered
# by the following copyright notice:
#
#---------------------------------------------------------------------------
#
# Copyright (c) 1996 Unicode, Inc.  All Rights reserved.
#
# This file is provided as-is by Unicode, Inc. (The Unicode Consortium).
# No claims are made as to fitness for any particular purpose.  No
# warranties of any kind are expressed or implied.  The recipient
# agrees to determine applicability of information provided.  If this
# file has been provided on magnetic media by Unicode, Inc., the sole
# remedy for any claim will be exchange of defective media within 90
# days of receipt.
#
# Recipient is granted the right to make copies in any form for
# internal distribution and to freely use the information supplied
# in the creation of products supporting Unicode.  Unicode, Inc.
# specifically excludes the right to re-distribute this file directly
# to third parties or other organizations whether for profit or not.
#
# In other words:  Don't put this file on the Internet.  People who want to 
# get it over the Internet should do so directly from ftp://unicode.org.  They
# can therefore be assured of getting the most recent and accurate version.
#
#----------------------------------------------------------------------------
#
# The txt2enc program built by this makefile is used to compile individual
# .txt files into .enc files, the format that Tcl understands for encoding 
# files.  This compilation to a different format is allowed by the above
# restriction. 
#
# The files shiftjis.txt and jis0208.txt were modified from the original
# ones provided on the Unicode CD.  The double-width backslash character
# 0x815F in these two Japanese encodings was being mapped to Unicode 005C
# (REVERSE SOLIDUS), the normal backslash character.  They have been
# changed to map 0x815F to Unicode FF3C (FULLWIDTH REVERSE SOLIDUS) and let
# the regular backslash character map to itself.  This follows how cp932
# behaves.
#
# Copyright (c) 1998 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# SCCS: @(#) Makefile 1.1 98/01/28 11:41:36
#

EUC_ENCODINGS = euc-cn.txt euc-kr.txt euc-jp.txt 

encodings: clean txt2enc $(EUC_ENCODINGS)
	@echo Compiling encoding files.
	@for p in *.esc; do \
	    base=`echo $$p | sed 's/\..*$$//'`; \
	    echo $$base.enc; \
	    echo "# Encoding file: $$base, escape-driven" > $$base.enc; \
	    echo "E" >> $$base.enc; \
	    cat $$p >> $$base.enc; \
	done
	@for p in *.txt; do \
	    enc=`echo $$p | sed 's/\..*$$/\.enc/'`; \
	    echo $$enc; \
	    ./txt2enc -e 0 -u 1 $$p > $$enc; \
	done
	@echo 
	@echo Compiling special versions of encoding files.
	@for p in ascii.txt; do \
	    enc=`echo $$p | sed 's/\..*$$/\.enc/'`; \
	    echo $$enc; \
	    ./txt2enc -m $$p > $$enc; \
	done
	@for p in jis0208.txt; do \
	    enc=`echo $$p | sed 's/\..*$$/\.enc/'`; \
	    echo $$enc; \
	    ./txt2enc -e 1 -u 2 $$p > $$enc; \
	done
	@for p in symbol.txt dingbats.txt macDingbats.txt; do \
	    enc=`echo $$p | sed 's/\..*$$/\.enc/'`; \
	    echo $$enc; \
	    ./txt2enc -e 0 -u 1 -s $$p > $$enc; \
	done

clean:
	@rm -f txt2enc *.enc $(EUC_ENCODINGS)

txt2enc: txt2enc.c
	@gcc -o txt2enc txt2enc.c

euc-jp.txt: ascii.txt jis0208.txt Makefile
	@echo Building euc-jp.txt from components.
	@cat ascii.txt > euc-jp.txt
	@grep '^0x[A-F]' jis0201.txt | sed 's/0x/0x8E/' >> euc-jp.txt
	@cat jis0208.txt | awk 'BEGIN {print "ibase=16"} index($$1,"#") != 1 {print substr($$2,3) "+" 8080; print substr($$3,3)}' | bc | awk '{ str = $$1; getline; printf "0x%04x 0x%04x\n", str, $$0}' >> euc-jp.txt

euc-kr.txt: ascii.txt ksc5601.txt Makefile
	@echo Building euc-kr.txt from components.
	@cat ascii.txt > euc-kr.txt
	@cat ksc5601.txt | awk 'BEGIN {print "ibase=16"} index($$1,"#") != 1 {print substr($$1,3) "+" 8080; print substr($$2,3)}' | bc | awk '{ str = $$1; getline; printf "0x%04x 0x%04x\n", str, $$0}' >> euc-kr.txt

euc-cn.txt: ascii.txt gb2312.txt Makefile
	@echo Building euc-cn.txt from components.
	@cat ascii.txt > euc-cn.txt
	@cat gb2312.txt | awk 'BEGIN {print "ibase=16"} index($$1,"#") != 1 {print substr($$1,3) "+" 8080; print substr($$2,3)}' | bc | awk '{ str = $$1; getline; printf "0x%04x 0x%04x\n", str, $$0}' >> euc-cn.txt
