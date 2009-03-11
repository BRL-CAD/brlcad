
# This code was developed with the support of the United States Government,
# and is not subject to copyright.

	       CC = g++
                M = SUN4
                X = X11
               RM = rm -f
             MAKE = make X=$X
            SHELL = /bin/sh

      CURRENT_DIR = .
	   OFILES = $(CURRENT_DIR)/ofiles
    CURRENT_PROBE = $(CURRENT_DIR)/dp

.KEEP_STATE:

lib::
	-@if [ -d $(OFILES) ]; then \
	echo " "; \
	echo "running make lib" "in $(OFILES)"; \
	cd $(OFILES); $(MAKE) lib; \
	else exit 0; fi

dp::  
	-@if [ -d $(CURRENT_PROBE) ]; then \
	echo " "; \
	echo "making all" "in $(CURRENT_PROBE)"; \
	cd $(CURRENT_PROBE); $(MAKE) all; \
	else exit 0; fi

all:: 	time depend lib dp time

time:
	@date

depend:
	-@if [ -d $(OFILES) ]; then \
	echo " "; \
	echo "running make depend" "in $(OFILES)"; \
	cd $(OFILES); $(MAKE) depend; \
	else exit 0; fi \
	-@if [ -d $(CURRENT_PROBE) ]; then \
	echo "running make depend" "in $(CURRENT_PROBE)"; \
	cd $(CURRENT_PROBE); $(MAKE) depend; \
	else exit 0; fi

