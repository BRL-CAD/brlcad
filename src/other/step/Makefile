PDES_ROOT = `pwd`
ARCH_DIR = arch

message:
	@echo " "
	@echo "You can run \"make default\" to install the NIST STEP Class Library and "
	@echo "the related EXPRESS Toolkit, EXPRESS Pretty Printer, and fedex_plus software"
	@echo "using defaults from your path."
	@echo "If... "
	@echo "- the installation fails or "
	@echo "- you didn't get the compilers, etc you expected or 
	@echo "- you want to do multiple installations (using different compilers or "
	@echo "  other SCL options)"
	@echo "then you will need to read the instructions in INSTALL"
	@echo " "

default:: time
	-@if [ -d $(ARCH_DIR) ]; then \
	echo " " ; \
	echo "***ERROR*** The directory \"$(ARCH_DIR)\" already exists. " ; \
	echo "            Move it aside or delete it or read INSTALL."; \
	echo " " ; \
	else \
	echo " " ; \
	echo "configure will create arch dir: \"$(ARCH_DIR)\"" ; \
	echo "running \"configure --with-arch=$(ARCH_DIR)\""; \
	configure --with-arch=$(ARCH_DIR); \
	echo " " ; \
	date ; \
	echo " " ; \
	fi

time:
	@echo " "
	@date

#  only use these make rules after the everything else has been installed

scl_examples: 
#  trigger when installing scl
	@echo "**************************************************************"
	@echo "***********  building scl example  ***********"
#  in this example the example schema is installed
	(mkProbe -l -i -R $(PDES_ROOT) data/example/example.exp example)
	@echo "**************************************************************"
	@echo "when you are through with the example, remove the directories"
	@echo "	    src/clSchemas/example and arch/Probes/example "
	@echo "**************************************************************"
	(cd src/test; make )
	@echo ""

dp_example:  
#  depends on running make install
	@echo "**************************************************************"
	@echo "***********  building example Data Probe  ***********"
#  in this example the example schema and Data Probe are not installed
	(mkProbe data/example/example.exp example)
	@echo "**************************************************************"
	@echo "when you are through with the example, remove the directory"
	@echo "	    example "
	@echo "**************************************************************"
	@echo ""

clean:	
	@echo "**************************************************************"
	@echo "***********  cleaning up  ***********"
	cd src/express; make clean; cd $(PDES_ROOT);
	cd src/exppp; make clean; cd $(PDES_ROOT);
	cd src/fedex_plus; make clean; cd $(PDES_ROOT);
	cd $(ARCH_DIR)/ofiles; make clean; cd $(PDES_ROOT);
#  remove the installed example
	cd $(ARCH_DIR)/Probes; 
	if [ -d example ]; then rm -r example; cd $(PDES_ROOT);
	cd src/clSchemas; 
	if [ -d example ]; then rm -r example; cd $(PDES_ROOT);

fresh:	clean
	@echo "building EXPRESS Toolkit libraries"
	cd src/express; co checkout; checkout; make install; cd $(PDES_ROOT);
	cd src/exppp; co checkout; checkout; make install; cd $(PDES_ROOT);
	@echo "building fedex_plus"
	cd src/fedex_plus; co checkout; 
	checkout; make depend; make install; cd $(PDES_ROOT);
	@echo "building the STEP Class Libraries"
	cd $(ARCH_DIR)/ofiles; make depend; make build; cd $(PDES_ROOT);

# To create a tar file use the following command.
# You need to do this from a seperate dir
# if you want to have a top level dir different from ~pdevel.

# For example create a release dir called SCL.
# In it place links to all the dirs you need.
# i.e. 
#		$(SCL)/src
#		$(SCL)/src/express
#		$(SCL)/src/exppp
#		$(SCL)/src/fedex_plus
#		$(SCL)/src/clstepcore
#		$(SCL)/src/clutils
#		$(SCL)/src/cleditor
#		$(SCL)/src/clprobe-ui
#
# and the bin, arch, man dir and any other files.
# Then run the tar command.  The options mean
#	c	create
#	v	verbose
#	h	hard code links as files
# 	f	file to create
#	FF	skip .o files and RCS files	
#	[X]	file name of other files to exclude

# create a tar file of the current dir
# by following all the links but don't copy any RCS dirs
#SCL  = DProbe3.0pre
#tarfile:
#	tar cvhfFF $(SCL).tar $(SCL)
#		$(SCL)/src/RCS
#		$(SCL)/src/express/RCS
#		$(SCL)/src/exppp/RCS
#		$(SCL)/src/fedex_plus/RCS
#		$(SCL)/src/clstepcore/RCS
#		$(SCL)/src/clutils/RCS
#		$(SCL)/src/cleditor/RCS
#		$(SCL)/src/clprobe-ui/RCS
#
#
#
# $(SCL)
