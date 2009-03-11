# $Id: Makefile,v 2.1.0.6 1997/10/20 23:19:22 sauderd Exp $

# This code was developed with the support of the United States Government,
# and is not subject to copyright.

MAKE_RULES =  ../make_rules
include $(MAKE_RULES)

             MAKE = make X=$X

      CURRENT_DIR = .

EDCORE = $(CURRENT_DIR)/editorCore
DAI = $(CURRENT_DIR)/dai
UTILS = $(CURRENT_DIR)/utils
IVFASD = $(CURRENT_DIR)/ivfasd
SCHEMACL = $(CURRENT_DIR)/schemacl
PROBE = $(CURRENT_DIR)/probe-ui
SDAIORBIX = $(CURRENT_DIR)/sdaiOrbix


all:: 	time lib schemacl time

time:
	@date

lib::	editorCore dai utils ivfasd probe-ui

editorCore::
	-@if [ -d $(EDCORE) ]; then \
	echo " "; \
	echo "making all" "in $(EDCORE)"; \
	cd $(EDCORE); $(MAKE) all; \
	else exit 0; fi

dai::
	-@if [ -d $(DAI) ]; then \
	echo " "; \
	echo "making all" "in $(DAI)"; \
	cd $(DAI); $(MAKE) all; \
	else exit 0; fi

utils::
	-@if [ -d $(UTILS) ]; then \
	echo " "; \
	echo "making all" "in $(UTILS)"; \
	cd $(UTILS); $(MAKE) all; \
	else exit 0; fi

sdaiOrbix::
	-@if [ -d $(SDAIORBIX) ]; then \
	echo " "; \
	echo "making all" "in $(SDAIORBIX)"; \
	cd $(SDAIORBIX); $(MAKE) message; \
	else exit 0; fi

ivfasd::
	-@if [ -d $(IVFASD) ]; then \
	echo " "; \
	echo "making all" "in $(IVFASD)"; \
	cd $(IVFASD); $(MAKE) all; \
	else exit 0; fi

probe-ui::
	-@if [ -d $(PROBE) ]; then \
	echo " "; \
	echo "making all" "in $(PROBE)"; \
	cd $(PROBE); $(MAKE) all; \
	else exit 0; fi

schemacl::  
	-@if [ -d $(SCHEMACL) ]; then \
	echo " "; \
	echo "making all" "in $(SCHEMACL)"; \
	cd $(SCHEMACL); $(MAKE) all; \
	else exit 0; fi

clean:  
	cd $(EDCORE); $(MAKE) clean; 
	cd $(UTILS); $(MAKE) clean; 
	cd $(DAI); $(MAKE) clean; 
	cd $(SDAIORBIX); $(MAKE) clean; 
	-@if [ -d $(IVFASD) ]; then \
	cd $(IVFASD); $(MAKE) clean; \
	else exit 0; fi
	-@if [ -d $(PROBE) ]; then \
	cd $(PROBE); $(MAKE) clean; \
	else exit 0; fi
	-@if [ -d $(SCHEMACL) ]; then \
	cd $(SCHEMACL); $(MAKE) clean; \
	else exit 0; fi

depend:  
	cd $(EDCORE); $(MAKE) depend; 
	cd $(DAI); $(MAKE) depend; 
	cd $(UTILS); $(MAKE) depend; 
	-@if [ -d $(IVFASD) ]; then \
	cd $(IVFASD); $(MAKE) depend; \
	else exit 0; fi
	-@if [ -d $(PROBE) ]; then \
	cd $(PROBE); $(MAKE) depend; \
	else exit 0; fi
	-@if [ -d $(SCHEMACL) ]; then \
	cd $(SCHEMACL); $(MAKE) depend; \
	else exit 0; fi

build:	checkout all
	
checkout:
	cd $(EDCORE); $(MAKE) checkout; 
	cd $(DAI); $(MAKE) checkout; 
	cd $(UTILS); $(MAKE) checkout; 
	-@if [ -d $(IVFASD) ]; then \
	cd $(IVFASD); $(MAKE) checkout; \
	else exit 0; fi
	-@if [ -d $(PROBE) ]; then \
	cd $(PROBE); $(MAKE) checkout; \
	else exit 0; fi
	-@if [ -d $(SCHEMACL) ]; then \
	cd $(SCHEMACL); $(MAKE) checkout; \
	else exit 0; fi
	
